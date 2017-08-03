#pragma once

#include <functional>

#include "extended_kalman_filter.h"

namespace kf {

/** \class UnscentedKalmanFilter unscented_kalman_filter.h
 * \brief Class containing functionality of unscented Kalman filter
 *
 * The process is described with equation q(k) = f(q(k-1), u(k-1))
 * The output is described with equation y(k) = h(q(k))
 *
 * Throughout the description "n" denotes dimension of state vector q, "m"
 * denotes dimension of output vector y (measured values), and "l" denotes
 * dimension of input vector u (control signals). Both prediction and correction
 * steps can be invoked separately.
 *
 * The class exploits Armadillo library for matrix operations and can throw any
 * of its exceptions (cf. www.arma.sourceforge.net).
*/
class UnscentedKalmanFilter : public ExtendedKalmanFilter
{
public:

  /** \brief Constructor with given dimensions
   *
   * Initializes all vectors and matrices to have correct dimensions. State
   * matrix as well as all covariance matrices are initialized with ones on
   * diagonal. The rest of data is zero-value initialized.
   *
   * \param dim_in is the dimension of input vector (number of control signals)
   * \param dim_out is the dimension of output vector (number of measurements)
   * \param dim_state is the dimension of state vector
  */
  UnscentedKalmanFilter(uint dim_in, uint dim_out, uint dim_state) :
    ExtendedKalmanFilter(dim_in, dim_out, dim_state),
    k_(2 * dim_state + 1),
    sigma_points_(k_, arma::vec(dim_state).zeros()),
    mean_weights_(k_, 0.0),
    covariance_weights_(k_, 0.0)
  {}

  /** \brief Constructor with given state-space matrices
   *
   * Initializes all vectors and matrices to have correct dimensions and assigns
   * given state-space matrices. All covariance matrices are initialized with
   * ones on diagonal. The rest of data is zero-value initialized.
   *
   * \param A is the state matrix
   * \param B is the input matrix
   * \param C is the output matrix
  */
  UnscentedKalmanFilter(const arma::mat& A, const arma::mat& B, const arma::mat& C) :
    ExtendedKalmanFilter(A, B, C),
    k_(2 * n_ + 1),
    sigma_points_(k_, arma::vec(n_).zeros()),
    mean_weights_(k_, 0.0),
    covariance_weights_(k_, 0.0)
  {}

  /** \brief Constructor with given state-space vectors
   *
   * Initializes all vectors and matrices to have correct dimensions and assigns
   * given vectors. Both predicted an estimated state vectors are assigned with
   * given state vector. All covariance matrices are initialized with ones on
   * diagonal. The rest of data is zero-value initialized.
   *
   * \param u is the input vector
   * \param y is the output vector
   * \param q is the state vector
  */
  UnscentedKalmanFilter(const arma::vec& u, const arma::vec& y, const arma::vec& q) :
    ExtendedKalmanFilter(u, y, q),
    k_(2 * n_ + 1),
    sigma_points_(k_, arma::vec(n_).zeros()),
    mean_weights_(k_, 0.0),
    covariance_weights_(k_, 0.0)
  {}


  /** \brief Performs the UKF prediction step
    *
    * TODO
    *
    * \sa processFunction()
   */
  virtual void predictState() {
    double alpha = 0.75;
    double beta = 0.2;
    double kappa = 3.0;
    double lambda = pow(alpha, 2.0) * (n_ + kappa) - n_;

    arma::mat sqrt_P = arma::chol((n_ + lambda) * P_);

    // 1. Calculate sigma points
    sigma_points_[0] = q_est_;
    mean_weights_[0] = lambda / (n_ + lambda);
    covariance_weights_[0] = lambda / (n_ + lambda) + (1 - pow(alpha, 2.0) + beta);

    for (int i = 1; i < n_ + 1; ++i) {
      sigma_points_[i] = q_est_ + sqrt_P.col(i - 1);
      sigma_points_[i + n_] = q_est_ - sqrt_P.col(i - 1);

      mean_weights_[i] = covariance_weights_[i] = 1.0 / (2.0 * (n_ + lambda));
      mean_weights_[i + n_] = covariance_weights_[i + n_] = 1.0 / (2.0 * (n_ + lambda));
    }

    // 2. Predict new points based on sigma points
    for (int i = 0; i < k_; ++i)
      pred_sigma_points_[i] = processFunction_(sigma_points_[i], u_);

    // 3. Calculate mean of predicted points
    q_pred_ = arma::vec(n_).zeros();
    for (int i = 0; i < k_; ++i) {
      q_pred_ += mean_weights_[i] * pred_sigma_points_[i];
    }

    // 4. Caclulate covariance of predicted points
    P_ = Q_;
    for (int i = 0; i < k_; ++i)
      P_ += covariance_weights_[i] * (pred_sigma_points_[i] - q_pred_) * trans(pred_sigma_points_[i] - q_pred_);
  }

  /** \brief Performs the UKF prediction step given the input vector
   *
   * TODO
   *
   * \param u is the input vector with dimension l
   * \sa KalmanFilter::setInput(), processFunction_()
  */
  virtual void predictState(const arma::vec& u) {
    setInput(u);
    predictState();
  }

  /** \brief Performs the UKF correction step
   *
   * TODO
   *
   * \sa outputFunction_()
  */
  virtual void correctState() {
    // 1. Calculate output points based on predicted sigma pts
    for (int i = 0; i < k_; ++i)
      output_sigma_points_[i] = outputFunction_(pred_sigma_points_[i]);

    arma::vec y_pred = arma::vec(m_).zeros();
    for (int i = 0; i < k_; ++i)
      y_pred += mean_weights_[i] * output_sigma_points_[i];

    S_ = R_;
    for (int i = 0; i < k_; ++i)
      S_ += covariance_weights_[i] * (output_sigma_points_[i] - y_pred) * trans(output_sigma_points_[i] - y_pred);

    arma::mat Sth = arma::mat(n_, m_).zeros();
    for (int i = 0; i < k_; ++i)
      Sth += covariance_weights_[i] * (pred_sigma_points_[i] - q_pred_) * trans(output_sigma_points_[i] - y_pred);

    K_ = Sth * inv(S_);

    q_est_ = q_pred_ + K_ * (y_ - y_pred);
    P_ = P_ - K_ * S_ * trans(K_);
  }

  /** \brief Performs the UKF correction step given the output vector
   *
   * TODO
   *
   * \sa KalmanFilter::setOutput(), outputFunction_()
  */
  virtual void correctState(const arma::vec& y) {
    setOutput(y);
    correctState();
  }

protected:
  /** \brief This class cannot be instantiated without providing dimensions. */
  UnscentedKalmanFilter() {}

private:
  /** \brief This base class member function is hidden because it serves no use
   * in this context.
  */
  void setProcessJacobian() {}

  /** \brief This base class member function is hidden because it serves no use
   * in this context.
  */
  void setOutputJacobian() {}

  uint k_;  /**< \brief Number of sigma points */

  std::vector<arma::vec> sigma_points_;         /**< \brief Points for distribution propagation */
  std::vector<arma::vec> pred_sigma_points_;    /**< \brief Points for distribution propagation */
  std::vector<arma::vec> output_sigma_points_;  /**< \brief Points for distribution propagation */

  std::vector<double> mean_weights_;        /**< \brief Weights for mean propagation */
  std::vector<double> covariance_weights_;  /**< \brief Weights for covariance propagation */
};

} // namespace kf
