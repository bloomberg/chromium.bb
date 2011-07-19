// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_ORIENTATION_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_ORIENTATION_H_

namespace device_orientation {
class Orientation {
 public:
  // alpha, beta and gamma are the rotations around the axes as specified in
  // http://dev.w3.org/geo/api/spec-source-orientation.html
  //
  // can_provide_{alpha,beta,gamma} is true if data can be provided for that
  // variable.

  Orientation(bool can_provide_alpha, double alpha,
              bool can_provide_beta, double beta,
              bool can_provide_gamma, double gamma)
      : alpha_(alpha),
        beta_(beta),
        gamma_(gamma),
        can_provide_alpha_(can_provide_alpha),
        can_provide_beta_(can_provide_beta),
        can_provide_gamma_(can_provide_gamma) {
  }

  Orientation()
      : alpha_(0),
        beta_(0),
        gamma_(0),
        can_provide_alpha_(false),
        can_provide_beta_(false),
        can_provide_gamma_(false) {
  }

  static Orientation Empty() { return Orientation(); }

  bool IsEmpty() const {
    return !can_provide_alpha_ && !can_provide_beta_ && !can_provide_gamma_;
  }

  double alpha_;
  double beta_;
  double gamma_;
  bool can_provide_alpha_;
  bool can_provide_beta_;
  bool can_provide_gamma_;
};

}  // namespace device_orientation

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_ORIENTATION_H_
