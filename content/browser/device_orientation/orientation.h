// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_ORIENTATION_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_ORIENTATION_H_

#include "base/compiler_specific.h"
#include "content/browser/device_orientation/device_data.h"
#include "content/common/content_export.h"

namespace content {

class Orientation : public DeviceData {
 public:
  // alpha, beta, gamma and absolute are the rotations around the axes as
  // specified in http://dev.w3.org/geo/api/spec-source-orientation.html
  //
  // can_provide_{alpha,beta,gamma,absolute} is true if data can be provided
  // for that variable.
  CONTENT_EXPORT Orientation();

  // From DeviceData.
  virtual IPC::Message* CreateIPCMessage(int render_view_id) const OVERRIDE;
  virtual bool ShouldFireEvent(const DeviceData* old_data) const OVERRIDE;

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
  virtual ~Orientation();

  static bool IsElementSignificantlyDifferent(bool can_provide_element1,
      bool can_provide_element2, double element1, double element2);

  double alpha_;
  double beta_;
  double gamma_;
  bool absolute_;
  bool can_provide_alpha_;
  bool can_provide_beta_;
  bool can_provide_gamma_;
  bool can_provide_absolute_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_ORIENTATION_H_
