// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_SOURCE_STATE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_SOURCE_STATE_H_

#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source_state.h"

namespace device {

// These are copies of various OpenVR mask constants that are used for
// specifying controller input. However, since we won't necessarily have OpenVR
// support compiled in, we can't use the constants directly.
// k_EButton_ApplicationMenu.
static constexpr uint64_t kMenuButton = 1ull << 1;
// k_EButton_Grip.
static constexpr uint64_t kGripButton = 1ull << 2;
// k_EButton_SteamVR_Touchpad.
static constexpr uint64_t kTrackpadButton = 1ull << 32;
// k_EButton_SteamVR_Trigger.
static constexpr uint64_t kSelectButton = 1ull << 33;
// k_EButton_Axis2.
static constexpr uint64_t kJoystickButton = 1ull << 34;
static constexpr uint64_t kTrackpadAxis = 0;
static constexpr uint64_t kSelectAxis = 1;
static constexpr uint64_t kJoystickAxis = 2;

class MockWMRInputSourceState : public WMRInputSourceState {
 public:
  MockWMRInputSourceState(ControllerFrameData data, unsigned int id);
  ~MockWMRInputSourceState() override;

  std::unique_ptr<WMRPointerPose> TryGetPointerPose(
      const WMRCoordinateSystem* origin) const override;
  std::unique_ptr<WMRInputSource> GetSource() const override;

  bool IsGrasped() const override;
  bool IsSelectPressed() const override;
  double SelectPressedValue() const override;

  bool SupportsControllerProperties() const override;

  bool IsThumbstickPressed() const override;
  bool IsTouchpadPressed() const override;
  bool IsTouchpadTouched() const override;
  double ThumbstickX() const override;
  double ThumbstickY() const override;
  double TouchpadX() const override;
  double TouchpadY() const override;

  std::unique_ptr<WMRInputLocation> TryGetLocation(
      const WMRCoordinateSystem* origin) const override;

 private:
  bool IsButtonPressed(uint64_t button_mask) const;
  ControllerFrameData data_;
  unsigned int id_;
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_SOURCE_STATE_H_
