// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_MOTION_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_MOTION_H_

#include "base/compiler_specific.h"
#include "content/browser/device_orientation/device_data.h"
#include "content/common/content_export.h"

namespace content {

class Motion : public DeviceData {
 public:
  // acceleration_x, acceleration_y, and acceleration_z are the accelerations
  // excluding gravity along the axes specified in
  // http://dev.w3.org/geo/api/spec-source-orientation.html

  // acceleration_including_gravity_x, acceleration_including_gravity_y, and
  // acceleration_including_gravity_z are the accelerations including gravity
  // along the same axes as above

  // rotation_rate_alpha, rotation_rate_beta, and rotataion_rate_gamma are the
  // rotations around the same axes as above

  // interval is the time interval at which data is obtained from the hardware,
  // as specified in the document referenced above

  // can_provide_{acceleration_x, acceleration_y, acceleration_z,
  // acceleration_including_gravity_x, acceleration_including_gravity_y,
  // acceleration_including_gravity_z, rotation_rate_alpha, rotation_rate_beta,
  // rotation_rate_gamma, interval} is true if data can be provided for that
  // variable
  CONTENT_EXPORT Motion();

  // From DeviceData.
  virtual IPC::Message* CreateIPCMessage(int render_view_id) const OVERRIDE;
  virtual bool ShouldFireEvent(const DeviceData* old_data) const OVERRIDE;

  void set_acceleration_x(double acceleration_x) {
    can_provide_acceleration_x_ = true;
    acceleration_x_ = acceleration_x;
  }
  bool can_provide_acceleration_x() const {
    return can_provide_acceleration_x_;
  }
  double acceleration_x() const { return acceleration_x_; }

  void set_acceleration_y(double acceleration_y) {
    can_provide_acceleration_y_ = true;
    acceleration_y_ = acceleration_y;
  }
  bool can_provide_acceleration_y() const {
    return can_provide_acceleration_y_;
  }
  double acceleration_y() const { return acceleration_y_; }

  void set_acceleration_z(double acceleration_z) {
    can_provide_acceleration_z_ = true;
    acceleration_z_ = acceleration_z;
  }
  bool can_provide_acceleration_z() const {
    return can_provide_acceleration_z_;
  }
  double acceleration_z() const { return acceleration_z_; }

  void set_acceleration_including_gravity_x(
      double acceleration_including_gravity_x) {
    can_provide_acceleration_including_gravity_x_ = true;
    acceleration_including_gravity_x_ = acceleration_including_gravity_x;
  }
  bool can_provide_acceleration_including_gravity_x() const {
    return can_provide_acceleration_x_;
  }
  double acceleration_including_gravity_x() const {
    return acceleration_including_gravity_x_;
  }

  void set_acceleration_including_gravity_y(
      double acceleration_including_gravity_y) {
    can_provide_acceleration_including_gravity_y_ = true;
    acceleration_including_gravity_y_ = acceleration_including_gravity_y;
  }
  bool can_provide_acceleration_including_gravity_y() const {
    return can_provide_acceleration_y_;
  }
  double acceleration_including_gravity_y() const {
    return acceleration_including_gravity_y_;
  }

  void set_acceleration_including_gravity_z(
      double acceleration_including_gravity_z) {
    can_provide_acceleration_including_gravity_z_ = true;
    acceleration_including_gravity_z_ = acceleration_including_gravity_z;
  }
  bool can_provide_acceleration_including_gravity_z() const {
    return can_provide_acceleration_z_;
  }
  double acceleration_including_gravity_z() const {
    return acceleration_including_gravity_z_;
  }

  void set_rotation_rate_alpha(double rotation_rate_alpha) {
    can_provide_rotation_rate_alpha_ = true;
    rotation_rate_alpha_ = rotation_rate_alpha;
  }
  bool can_provide_rotation_rate_alpha() const {
    return can_provide_rotation_rate_alpha_;
  }
  double rotation_rate_alpha() const { return rotation_rate_alpha_; }

  void set_rotation_rate_beta(double rotation_rate_beta) {
    can_provide_rotation_rate_beta_ = true;
    rotation_rate_beta_ = rotation_rate_beta;
  }
  bool can_provide_rotation_rate_beta() const {
    return can_provide_rotation_rate_beta_;
  }
  double rotation_rate_beta() const { return rotation_rate_beta_; }

  void set_rotation_rate_gamma(double rotation_rate_gamma) {
    can_provide_rotation_rate_gamma_ = true;
    rotation_rate_gamma_ = rotation_rate_gamma;
  }
  bool can_provide_rotation_rate_gamma() const {
    return can_provide_rotation_rate_gamma_;
  }
  double rotation_rate_gamma() const { return rotation_rate_gamma_; }

  void set_interval(double interval) {
    can_provide_interval_ = true;
    interval_ = interval;
  }
  bool can_provide_interval() const { return can_provide_interval_; }
  double interval() const { return interval_; }

 private:
  virtual ~Motion();

  double acceleration_x_;
  double acceleration_y_;
  double acceleration_z_;
  double acceleration_including_gravity_x_;
  double acceleration_including_gravity_y_;
  double acceleration_including_gravity_z_;
  double rotation_rate_alpha_;
  double rotation_rate_beta_;
  double rotation_rate_gamma_;
  double interval_;
  bool can_provide_acceleration_x_;
  bool can_provide_acceleration_y_;
  bool can_provide_acceleration_z_;
  bool can_provide_acceleration_including_gravity_x_;
  bool can_provide_acceleration_including_gravity_y_;
  bool can_provide_acceleration_including_gravity_z_;
  bool can_provide_rotation_rate_alpha_;
  bool can_provide_rotation_rate_beta_;
  bool can_provide_rotation_rate_gamma_;
  bool can_provide_interval_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_MOTION_H_
