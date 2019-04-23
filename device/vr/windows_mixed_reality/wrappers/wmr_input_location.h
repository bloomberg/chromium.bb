// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_LOCATION_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_LOCATION_H_

#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include "base/macros.h"

namespace device {
class WMRInputLocation {
 public:
  explicit WMRInputLocation(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation>
          location);
  virtual ~WMRInputLocation();

  // Uses ISpatialInteractionSourceLocation.
  bool TryGetPosition(
      ABI::Windows::Foundation::Numerics::Vector3* position) const;
  bool TryGetVelocity(
      ABI::Windows::Foundation::Numerics::Vector3* velocity) const;

  // Uses ISpatialInteractionSourceLocation2.
  bool TryGetOrientation(
      ABI::Windows::Foundation::Numerics::Quaternion* orientation) const;

  // Uses ISpatialInteractionSourceLocation3.
  bool TryGetAngularVelocity(
      ABI::Windows::Foundation::Numerics::Vector3* angular_velocity) const;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation>
      location_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation2>
      location2_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation3>
      location3_;

  DISALLOW_COPY(WMRInputLocation);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_LOCATION_H_
