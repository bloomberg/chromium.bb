// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_STATE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_STATE_H_

#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>
#include <wrl.h>

namespace device {
class WMRCoordinateSystem;
class WMRInputLocation;
class WMRInputSource;
class WMRPointerPose;
class WMRInputSourceState {
 public:
  explicit WMRInputSourceState(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState>
          source_state);
  WMRInputSourceState(const WMRInputSourceState& other);
  virtual ~WMRInputSourceState();

  // Uses ISpatialInteractionSourceState.
  bool TryGetPointerPose(const WMRCoordinateSystem* origin,
                         WMRPointerPose* pointer_pose) const;
  WMRInputSource GetSource() const;

  // Uses ISpatialInteractionSourceState2.
  bool IsGrasped() const;
  bool IsSelectPressed() const;
  double SelectPressedValue() const;

  bool SupportsControllerProperties() const;

  // Uses SpatialInteractionControllerProperties.
  bool IsThumbstickPressed() const;
  bool IsTouchpadPressed() const;
  bool IsTouchpadTouched() const;
  double ThumbstickX() const;
  double ThumbstickY() const;
  double TouchpadX() const;
  double TouchpadY() const;

  // Uses SpatialInteractionSourceProperties.
  bool TryGetLocation(const WMRCoordinateSystem* origin,
                      WMRInputLocation* location) const;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState>
      source_state_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState2>
      source_state2_;

  // Typically we want to restrict each wrapper to one "COM" class, but this is
  // a Property on SpatialInteractionSourceState that is just a struct.
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionControllerProperties>
      controller_properties_;

  // We only use one method from this class, which is a property on our class.
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceProperties>
      properties_;
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_STATE_H_
