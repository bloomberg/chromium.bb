// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_pose.h"

#include <windows.perception.h>
#include <windows.ui.input.spatial.h>

#include <wrl.h>

#include "base/logging.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_source_pose.h"

namespace WFN = ABI::Windows::Foundation::Numerics;
using ABI::Windows::Perception::People::IHeadPose;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerPose;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerPose2;
using Microsoft::WRL::ComPtr;

namespace device {
WMRPointerPose::WMRPointerPose(ComPtr<ISpatialPointerPose> pointer_pose)
    : pointer_pose_(pointer_pose) {
  if (pointer_pose_) {
    pointer_pose_.As(&pointer_pose2_);

    HRESULT hr = pointer_pose_->get_Head(&head_);
    DCHECK(SUCCEEDED(hr));
  }
}

WMRPointerPose::~WMRPointerPose() = default;

bool WMRPointerPose::IsValid() const {
  return pointer_pose_ != nullptr;
}

bool WMRPointerPose::TryGetInteractionSourcePose(
    const WMRInputSource& source,
    WMRPointerSourcePose* pointer_source_pose) const {
  DCHECK(pointer_source_pose);
  if (!pointer_pose2_)
    return false;

  ComPtr<ISpatialPointerInteractionSourcePose> psp_wmr;
  HRESULT hr = pointer_pose2_->TryGetInteractionSourcePose(
      source.GetComPtr().Get(), &psp_wmr);
  if (SUCCEEDED(hr) && psp_wmr) {
    *pointer_source_pose = WMRPointerSourcePose(psp_wmr);
    return true;
  }

  return false;
}

WFN::Vector3 WMRPointerPose::HeadForward() const {
  DCHECK(IsValid());
  WFN::Vector3 val;
  HRESULT hr = head_->get_ForwardDirection(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

}  // namespace device
