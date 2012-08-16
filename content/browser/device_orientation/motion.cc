// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/motion.h"

#include "content/common/device_motion_messages.h"

namespace content {

Motion::Motion()
    : can_provide_acceleration_x_(false),
      can_provide_acceleration_y_(false),
      can_provide_acceleration_z_(false),
      can_provide_acceleration_including_gravity_x_(false),
      can_provide_acceleration_including_gravity_y_(false),
      can_provide_acceleration_including_gravity_z_(false),
      can_provide_rotation_rate_alpha_(false),
      can_provide_rotation_rate_beta_(false),
      can_provide_rotation_rate_gamma_(false),
      can_provide_interval_(false) {
}

Motion::~Motion() {
}

IPC::Message* Motion::CreateIPCMessage(int render_view_id) const {
  DeviceMotionMsg_Updated_Params params;

  params.can_provide_acceleration_x = can_provide_acceleration_x_;
  params.acceleration_x = acceleration_x_;
  params.can_provide_acceleration_y = can_provide_acceleration_y_;
  params.acceleration_y = acceleration_y_;
  params.can_provide_acceleration_z = can_provide_acceleration_z_;
  params.acceleration_z = acceleration_z_;

  params.can_provide_acceleration_including_gravity_x =
      can_provide_acceleration_including_gravity_x_;
  params.acceleration_including_gravity_x = acceleration_including_gravity_x_;
  params.can_provide_acceleration_including_gravity_y =
      can_provide_acceleration_including_gravity_y_;
  params.acceleration_including_gravity_y = acceleration_including_gravity_y_;
  params.can_provide_acceleration_including_gravity_z =
      can_provide_acceleration_including_gravity_z_;
  params.acceleration_including_gravity_z = acceleration_including_gravity_z_;

  params.can_provide_rotation_rate_alpha = can_provide_rotation_rate_alpha_;
  params.rotation_rate_alpha = rotation_rate_alpha_;
  params.can_provide_rotation_rate_beta = can_provide_rotation_rate_beta_;
  params.rotation_rate_beta = rotation_rate_beta_;
  params.can_provide_rotation_rate_gamma = can_provide_rotation_rate_gamma_;
  params.rotation_rate_gamma = rotation_rate_gamma_;

  params.can_provide_interval = can_provide_interval_;
  params.interval = interval_;

  return new DeviceMotionMsg_Updated(render_view_id, params);
}

// Should always fire new motion events so that they occur at regular intervals.
// The firing frequency is determined by the polling frequency in ProviderImpl.
bool Motion::ShouldFireEvent(const DeviceData* old_data) const {
  return true;
}

};  // namespace content
