// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_POINTER_SOURCE_POSE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_POINTER_SOURCE_POSE_H_

#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include "base/macros.h"

namespace device {
class WMRPointerSourcePose {
 public:
  explicit WMRPointerSourcePose(
      Microsoft::WRL::ComPtr<ABI::Windows::UI::Input::Spatial::
                                 ISpatialPointerInteractionSourcePose>
          pointer_pose);
  virtual ~WMRPointerSourcePose();

  bool IsValid() const;

  // Uses ISpatialPointerInteractionSourcePose.
  ABI::Windows::Foundation::Numerics::Vector3 Position() const;

  // Uses ISpatialPointerInteractionSourcePose2.
  ABI::Windows::Foundation::Numerics::Quaternion Orientation() const;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose>
      pointer_source_pose_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose2>
      pointer_source_pose2_;

  DISALLOW_COPY(WMRPointerSourcePose);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_POINTER_SOURCE_POSE_H_
