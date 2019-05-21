// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_source_state.h"

#include "base/logging.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_location.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_source.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_pointer_pose.h"

namespace device {

MockWMRInputSourceState::MockWMRInputSourceState(ControllerFrameData data,
                                                 unsigned int id)
    : data_(data), id_(id) {}

MockWMRInputSourceState::~MockWMRInputSourceState() = default;

std::unique_ptr<WMRPointerPose> MockWMRInputSourceState::TryGetPointerPose(
    const WMRCoordinateSystem* origin) const {
  return std::make_unique<MockWMRPointerPose>();
}

std::unique_ptr<WMRInputSource> MockWMRInputSourceState::GetSource() const {
  return std::make_unique<MockWMRInputSource>(data_, id_);
}

bool MockWMRInputSourceState::IsGrasped() const {
  return IsButtonPressed(kGripButton);
}

bool MockWMRInputSourceState::IsSelectPressed() const {
  return IsButtonPressed(kSelectButton);
}

double MockWMRInputSourceState::SelectPressedValue() const {
  double val = data_.axis_data[kSelectAxis].x;
  // Should only be in [0, 1] for triggers.
  DCHECK(val <= 1);
  DCHECK(val >= 0);
  return val;
}

bool MockWMRInputSourceState::SupportsControllerProperties() const {
  return true;
}

bool MockWMRInputSourceState::IsThumbstickPressed() const {
  return IsButtonPressed(kJoystickButton);
}

bool MockWMRInputSourceState::IsTouchpadPressed() const {
  return IsButtonPressed(kTrackpadButton);
}

bool MockWMRInputSourceState::IsTouchpadTouched() const {
  auto touched =
      data_.supported_buttons & data_.buttons_touched & kTrackpadButton;
  return touched != 0;
}

double MockWMRInputSourceState::ThumbstickX() const {
  double val = data_.axis_data[kJoystickAxis].x;
  // Should be in [-1, 1] for joysticks.
  DCHECK(val <= 1);
  DCHECK(val >= -1);
  return val;
}

double MockWMRInputSourceState::ThumbstickY() const {
  double val = data_.axis_data[kJoystickAxis].y;
  // Should be in [-1, 1] for joysticks.
  DCHECK(val <= 1);
  DCHECK(val >= -1);
  return val;
}

double MockWMRInputSourceState::TouchpadX() const {
  double val = data_.axis_data[kTrackpadAxis].x;
  // Should be in [-1, 1] for touchpads.
  DCHECK(val <= 1);
  DCHECK(val >= -1);
  return val;
}

double MockWMRInputSourceState::TouchpadY() const {
  double val = data_.axis_data[kTrackpadAxis].y;
  // Should be in [-1, 1] for touchpads.
  DCHECK(val <= 1);
  DCHECK(val >= -1);
  return val;
}

std::unique_ptr<WMRInputLocation> MockWMRInputSourceState::TryGetLocation(
    const WMRCoordinateSystem* origin) const {
  return std::make_unique<MockWMRInputLocation>(data_);
}

bool MockWMRInputSourceState::IsButtonPressed(uint64_t button_mask) const {
  auto pressed = data_.supported_buttons & data_.buttons_pressed & button_mask;
  return pressed != 0;
}

}  // namespace device
