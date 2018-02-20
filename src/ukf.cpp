//
// Created by Isaac on 10/28/2017.
//

#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
    // if this is false, laser measurements will be ignored (except during init)
    use_laser_ = true;

    // if this is false, radar measurements will be ignored (except during init)
    use_radar_ = true;

    // initial state vector
    x_ = VectorXd(5);

    // initial covariance matrix
    P_ = MatrixXd(5, 5);

    // Process noise standard deviation longitudinal acceleration in m/s^2
    std_a_ = 1.51; //1.5

    // Process noise standard deviation yaw acceleration in rad/s^2
    std_yawdd_ = .51; //.51

    // Laser measurement noise standard deviation position1 in m
    std_laspx_ = 0.15;

    // Laser measurement noise standard deviation position2 in m
    std_laspy_ = 0.15;

    // Radar measurement noise standard deviation radius in m
    std_radr_ = 0.3;

    // Radar measurement noise standard deviation angle in rad
    std_radphi_ = 0.03;

    // Radar measurement noise standard deviation radius change in m/s
    std_radrd_ = 0.3;

    /**
    TODO:

    Complete the initialization. See ukf.h for other member properties.

    Hint: one or more values initialized above might be wildly off...
    */
    n_x_ = 5;
    n_aug_ = 7;
    lambda_ = 3 - n_x_;
    is_initialized_ = false;
    time_us_ = 0.0;
    Xsig_pred_ = MatrixXd(n_x_, 2*n_aug_ + 1);
    weights_ = VectorXd(2*n_aug_ + 1);

//    R_radar = MatrixXd(3,3);
//    R_radar.fill(0.0);
//    R_radar(0,0) = std_radr_*std_radr_;
//    R_radar(1,1) = std_radphi_*std_radphi_;
//    R_radar(2,2) = std_radrd_*std_radrd_;
//
//    R_lidar = MatrixXd(2,2);
//    R_lidar.fill(0.0);
//    R_lidar(0,0) = std_laspx_*std_laspx_;
//    R_lidar(1,1) = std_laspy_*std_laspy_;

    NIS_laser_ = 0.0;
    NIS_radar_ = 0.0;

}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
    /**
    TODO:

    Complete this function! Make sure you switch between lidar and radar
    measurements.
    */

    if((meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_)||
       (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_)){

        /*****************************************************************************
       *  initialization
       ****************************************************************************/

        if(!is_initialized_){

            // initialize state vector
            x_ << 1,1,1,1,0.1;

            // initialize covariance matrix
            P_ <<  0.15,0,  0,  0,  0,
                    0,0.15,   0,  0,  0,
                    0,  0,  1,  0,  0,
                    0,  0,  0,  1,  0,
                    0,  0,  0,  0,  1;


            // initialize timestamp
            time_us_ = meas_package.timestamp_;

            // initialize position based on first measurement
            if(meas_package.sensor_type_ == MeasurementPackage::LASER){

                x_(0) = meas_package.raw_measurements_(0);
                x_(1) = meas_package.raw_measurements_(1);

            } else if(meas_package.sensor_type_ == MeasurementPackage::RADAR){

                float ro = meas_package.raw_measurements_(0);
                float phi = meas_package.raw_measurements_(1);
//                float ro_d = meas_package.raw_measurements_(2);
//                float vx = ro_d*cos(phi);
//                float vy = ro_d*sin(phi);

                // convert to cartesian coordinates
                x_(0) = ro * cos(phi);
                x_(1) = ro * sin(phi);
//                x_(2) = sqrt(vx*vx + vy*vy);
            }

            // initialize just in first measurement
            is_initialized_ = true;
            return;
        }
        /*****************************************************************************
        *  Prediction
        ****************************************************************************/

        float dt = (meas_package.timestamp_ - time_us_) / 1000000.0;
        time_us_ = meas_package.timestamp_;

        Prediction(dt);

        /*****************************************************************************
       *  Update
       ****************************************************************************/

        if (meas_package.sensor_type_ == MeasurementPackage::LASER){
            UpdateLidar(meas_package);
        }else if (meas_package.sensor_type_ == MeasurementPackage::RADAR){
            UpdateRadar(meas_package);
        }

    }

}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
    /**
    TODO:

    Complete this function! Estimate the object's location. Modify the state
    vector, x_. Predict sigma points, the state, and the state covariance matrix.
    */

    /*****************************************************************************
    *  Generate Sigma Points
    ****************************************************************************/

    //create sigma point matrix
    MatrixXd Xsig = MatrixXd(n_x_, 2 * n_x_ + 1);

    //calculate square root of P
    MatrixXd A = P_.llt().matrixL();

    //set lambda for non-augmented sigma points
    lambda_ = 3 - n_x_;

    //set first column of sigma point matrix
    Xsig.col(0)  = x_;

    //set remaining sigma points
    for (int i = 0; i < n_x_; i++)
    {
        Xsig.col(i+1)     = x_ + sqrt(lambda_+n_x_) * A.col(i);
        Xsig.col(i+1+n_x_) = x_ - sqrt(lambda_+n_x_) * A.col(i);
    }

    /*****************************************************************************
    *  Augment Sigma Points
    ****************************************************************************/

    //create augmented mean vector
    VectorXd x_aug = VectorXd(n_aug_);

    //create augmented state covariance
    MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);

    //create sigma point matrix
    MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);

    //set lambda for augmented sigma points
    lambda_ = 3 - n_aug_;

    //create augmented mean state
    x_aug.head(5) = x_;
    x_aug(5) = 0;
    x_aug(6) = 0;

    //create augmented covariance matrix
    P_aug.fill(0.0);
    P_aug.topLeftCorner(5,5) = P_;
    P_aug(5,5) = std_a_*std_a_;
    P_aug(6,6) = std_yawdd_*std_yawdd_;

    //create square root matrix
    MatrixXd L = P_aug.llt().matrixL();

    //create augmented sigma points
    Xsig_aug.col(0)  = x_aug;

    float lambda_naug = sqrt(lambda_+n_aug_);

    for (int i = 0; i< n_aug_; i++)
    {
        Xsig_aug.col(i+1)       = x_aug + lambda_naug * L.col(i);
        Xsig_aug.col(i+1+n_aug_) = x_aug - lambda_naug * L.col(i);
    }

    /*****************************************************************************
    *  Predict Sigma Points
    ****************************************************************************/

    //create matrix with predicted sigma points as columns
//    MatrixXd Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

    //predict sigma points
    for (int i = 0; i< 2*n_aug_+1; i++)
    {
        //extract values for better readability
        double p_x      = Xsig_aug(0,i);
        double p_y      = Xsig_aug(1,i);
        double v        = Xsig_aug(2,i);
        double yaw      = Xsig_aug(3,i);
        double yawd     = Xsig_aug(4,i);
        double nu_a     = Xsig_aug(5,i);
        double nu_yawdd = Xsig_aug(6,i);

        //predicted state values
        double px_p, py_p;

        //avoid division by zero
        if (fabs(yawd) > 0.001) {
            px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
            py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
        }
        else {
            px_p = p_x + v*delta_t*cos(yaw);
            py_p = p_y + v*delta_t*sin(yaw);
        }

        double v_p = v;
        double yaw_p = yaw + yawd*delta_t;
        double yawd_p = yawd;

        //add noise
        px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
        py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
        v_p = v_p + nu_a*delta_t;

        yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
        yawd_p = yawd_p + nu_yawdd*delta_t;

        //write predicted sigma point into right column
        Xsig_pred_(0,i) = px_p;
        Xsig_pred_(1,i) = py_p;
        Xsig_pred_(2,i) = v_p;
        Xsig_pred_(3,i) = yaw_p;
        Xsig_pred_(4,i) = yawd_p;
    }

    /*****************************************************************************
    *  Convert Predicted Sigma Points to Mean/Covariance
    ****************************************************************************/

    // set weights
    double weight_0 = lambda_ / (lambda_ + n_aug_);
    weights_(0) = weight_0;
    for (int i = 1; i < 2 * n_aug_ + 1; i++) {  //2n+1 weights
        double weight = 0.5 / (n_aug_ + lambda_);
        weights_(i) = weight;
    }

    //predict state mean
    x_.fill(0.0);
    for(int i=0; i < 2 * n_aug_ + 1; i++){
        x_ += weights_(i)*Xsig_pred_.col(i);
    }
    //predict state covariance matrix
    P_.fill(0.0);
    for(int i=0; i < 2 * n_aug_ + 1; i++){

        // state difference
        VectorXd x_diff = Xsig_pred_.col(i) - x_;

        //angle normalization
        while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
        while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

        P_ = P_ + weights_(i) * x_diff * x_diff.transpose();

    }

}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
    /**
    TODO:

    Complete this function! Use lidar data to update the belief about the object's
    position. Modify the state vector, x_, and covariance, P_.

    You'll also need to calculate the lidar NIS.
    */

    //set measurement dimension, lidar can measure px, py
    int n_z_ = 2;

    //create matrix for sigma points in measurement space
    MatrixXd Zsig = MatrixXd(n_z_, 2 * n_aug_ + 1);

//transform sigma points into measurement space

    for(int i=0; i < 2 * n_aug_ + 1; i++){

        double p_x = Xsig_pred_(0, i);
        double p_y = Xsig_pred_(1, i);

        Zsig(0, i) = p_x;
        Zsig(1, i) = p_y;
    }

    //mean predicted measurement
    VectorXd z_pred = VectorXd(n_z_);
    z_pred.fill(0.0);
    for (int i = 0 ; i < 2 * n_aug_ + 1; i++){
        z_pred = z_pred + weights_(i) * Zsig.col(i);
    }

    //measurement covariance matrix S
    MatrixXd S = MatrixXd(n_z_,n_z_);
    S.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;

        S = S + weights_(i) * z_diff * z_diff.transpose();
    }

    //add measurement noise covariance matrix
    MatrixXd R = MatrixXd(n_z_, n_z_);
    R << std_laspx_*std_laspx_, 0,
            0, std_laspy_*std_laspy_;
    S = S + R;

    /*****************************************************************************
    *  UKF Update for Lidar
    ****************************************************************************/

    //create matrix for cross correlation Tc
    MatrixXd Tc = MatrixXd(n_x_, n_z_);

    //calculate cross correlation matrix
    Tc.fill(0.0);
    for(int i=0; i < 2 * n_aug_ + 1; i++){

        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;

        // state difference
        VectorXd x_diff = Xsig_pred_.col(i) - x_;

        Tc = Tc + weights_(i) * x_diff * z_diff.transpose();

    }

    //calculate Kalman gain K;
    MatrixXd K = Tc*S.inverse();

    //update state mean and covariance matrix
    VectorXd z_ = meas_package.raw_measurements_;
    VectorXd z_diff = z_ - z_pred;

    //calculate NIS
    NIS_laser_ = z_diff.transpose() * S.inverse() * z_diff;

    x_ = x_ + K*z_diff;
    P_ = P_ - K*S*K.transpose();

}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
    /**
    TODO:

    Complete this function! Use radar data to update the belief about the object's
    position. Modify the state vector, x_, and covariance, P_.

    You'll also need to calculate the radar NIS.
    */

    //set measurement dimension, radar can measure r, phi, and r_dot
    int n_z_ = 3;

    //create matrix for sigma points in measurement space
    MatrixXd Zsig = MatrixXd(n_z_, 2 * n_aug_ + 1);

    //transform sigma points into measurement space

    for(int i=0; i < 2 * n_aug_ + 1; i++){

        double p_x = Xsig_pred_(0, i);
        double p_y = Xsig_pred_(1, i);
        double v = Xsig_pred_(2, i);
        double yaw = Xsig_pred_(3, i);

        double v1 = cos(yaw) * v;
        double v2 = sin(yaw) * v;

        // measurement model
        //ro
        Zsig(0, i) = sqrt(p_x * p_x + p_y * p_y);
        //phi
        Zsig(1, i) = atan2(p_y, p_x);
        //ro_dot
        Zsig(2, i) = (p_x * v1 + p_y * v2) / sqrt(p_x * p_x + p_y * p_y);
    }

    //calculate predicted measurement
    VectorXd z_pred = VectorXd(n_z_);
    z_pred.fill(0.0);
    for (int i=0; i < 2 * n_aug_ + 1; i++){
        z_pred = z_pred + weights_(i) * Zsig.col(i);
    }


    //measurement covariance matrix S
    MatrixXd S = MatrixXd(n_z_,n_z_);
    S.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;

        //angle normalization
        while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
        while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

        S = S + weights_(i) * z_diff * z_diff.transpose();
    }



    //add measurement noise covariance matrix
    MatrixXd R = MatrixXd(n_z_, n_z_);
    R << std_radr_*std_radr_,                       0,                     0,
            0, std_radphi_*std_radphi_,                     0,
            0,                       0, std_radrd_*std_radrd_;
    S = S + R;



    /*****************************************************************************
    *  UKF Update for Radar
    ****************************************************************************/

    //create matrix for cross correlation Tc
    MatrixXd Tc = MatrixXd(n_x_, n_z_);

    //calculate cross correlation matrix
    Tc.fill(0.0);
    for(int i=0; i < 2 * n_aug_ + 1; i++){

        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;
        //angle normalization
        while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
        while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

        // state difference
        VectorXd x_diff = Xsig_pred_.col(i) - x_;
        //angle normalization
        while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
        while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

        Tc = Tc + weights_(i) * x_diff * z_diff.transpose();

    }

    //calculate Kalman gain K;
    MatrixXd K = Tc*S.inverse();

    //update state mean and covariance matrix
    VectorXd z_ = meas_package.raw_measurements_;
    VectorXd z_diff = z_ - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

    NIS_radar_ = z_diff.transpose() * S.inverse() * z_diff;

    x_ = x_ + K*z_diff;
    P_ = P_ - K*S*K.transpose();

}



