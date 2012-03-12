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

  Orientation(bool can_provide_alpha, double alpha,
              bool can_provide_beta, double beta,
              bool can_provide_gamma, double gamma,
              bool can_provide_absolute, bool absolute)
      : alpha_(alpha),
        beta_(beta),
        gamma_(gamma),
        absolute_(absolute),
        can_provide_alpha_(can_provide_alpha),
        can_provide_beta_(can_provide_beta),
        can_provide_gamma_(can_provide_gamma),
        can_provide_absolute_(can_provide_absolute) {
  }

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

  static Orientation Empty() { return Orientation(); }

  bool IsEmpty() const {
    return !can_provide_alpha_ && !can_provide_beta_ && !can_provide_gamma_
        && !can_provide_absolute_;
  }

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
