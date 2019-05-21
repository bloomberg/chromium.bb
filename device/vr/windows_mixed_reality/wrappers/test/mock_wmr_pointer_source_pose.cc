// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_pointer_source_pose.h"

namespace device {

MockWMRPointerSourcePose::MockWMRPointerSourcePose() {}

MockWMRPointerSourcePose::~MockWMRPointerSourcePose() = default;

bool MockWMRPointerSourcePose::IsValid() const {
  return true;
}

ABI::Windows::Foundation::Numerics::Vector3 MockWMRPointerSourcePose::Position()
    const {
  // TODO(https://crbug.com/926048): Actually implement.
  return {1, 1, 1};
}

ABI::Windows::Foundation::Numerics::Quaternion
MockWMRPointerSourcePose::Orientation() const {
  // TODO(https://crbug.com/926048): Actually implement.
  // For whatever reason, W is first?
  return {1, 0, 0, 0};
}

}  // namespace device
