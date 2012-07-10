// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_ORIENTATION_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_ORIENTATION_H_

namespace device_orientation {
class Orientation {
 public:
  // alpha, beta, gamma and absolute are the rotations around the axes as
  // specified in http://dev.w3.org/geo/api/spec-source-orientation.html
  //
  // can_provide_{alpha,beta,gamma,absolute} is true if data can be provided
  // for that variable.

  Orientation()
      : alpha_(0),
        beta_(0),
        gamma_(0),
        absolute_(false),
        can_provide_alpha_(false),
        can_provide_beta_(false),
        can_provide_gamma_(false),
        can_provide_absolute_(false) {
  }
  Orientation(const Orientation& orientation)
      : alpha_(orientation.alpha()),
        beta_(orientation.beta()),
        gamma_(orientation.gamma()),
        absolute_(orientation.absolute()),
        can_provide_alpha_(orientation.can_provide_alpha()),
        can_provide_beta_(orientation.can_provide_beta()),
        can_provide_gamma_(orientation.can_provide_gamma()),
        can_provide_absolute_(orientation.can_provide_absolute()) {
  }
  void operator=(const Orientation& source) {
    alpha_ = source.alpha();
    beta_ = source.beta();
    gamma_ = source.gamma();
    absolute_ = source.absolute();
    can_provide_alpha_ = source.can_provide_alpha();
    can_provide_beta_ = source.can_provide_beta();
    can_provide_gamma_ = source.can_provide_gamma();
    can_provide_absolute_ = source.can_provide_absolute();
  }

  static Orientation Empty() { return Orientation(); }

  bool is_empty() const {
    return !can_provide_alpha_ && !can_provide_beta_ && !can_provide_gamma_
        && !can_provide_absolute_;
  }

  void set_alpha(double alpha) {
    can_provide_alpha_ = true;
    alpha_ = alpha;
  }
  bool can_provide_alpha() const { return can_provide_alpha_; }
  double alpha() const { return alpha_; }

  void set_beta(double beta) {
    can_provide_beta_ = true;
    beta_ = beta;
  }
  bool can_provide_beta() const { return can_provide_beta_; }
  double beta() const { return beta_; }

  void set_gamma(double gamma) {
    can_provide_gamma_ = true;
    gamma_ = gamma;
  }
  bool can_provide_gamma() const { return can_provide_gamma_; }
  double gamma() const { return gamma_; }

  void set_absolute(bool absolute) {
    can_provide_absolute_ = true;
    absolute_ = absolute;
  }
  bool can_provide_absolute() const { return can_provide_absolute_; }
  bool absolute() const { return absolute_; }

 private:
  double alpha_;
  double beta_;
  double gamma_;
  bool absolute_;
  bool can_provide_alpha_;
  bool can_provide_beta_;
  bool can_provide_gamma_;
  bool can_provide_absolute_;
};

}  // namespace device_orientation

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_ORIENTATION_H_
