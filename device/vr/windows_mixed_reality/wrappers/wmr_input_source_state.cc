// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source_state.h"

#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>

#include <wrl.h>

#include "base/logging.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_location.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_pose.h"

using ABI::Windows::UI::Input::Spatial::ISpatialInteractionControllerProperties;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceProperties;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState2;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerPose;
using Microsoft::WRL::ComPtr;

namespace device {
WMRInputSourceState::WMRInputSourceState(
    ComPtr<ISpatialInteractionSourceState> source_state)
    : source_state_(source_state) {
  DCHECK(source_state_);
  HRESULT hr = source_state_.As(&source_state2_);
  DCHECK(SUCCEEDED(hr));

  hr = source_state_->get_Properties(&properties_);
  DCHECK(SUCCEEDED(hr));

  source_state2_->get_ControllerProperties(&controller_properties_);
}

WMRInputSourceState::WMRInputSourceState(const WMRInputSourceState& other) =
    default;

WMRInputSourceState::~WMRInputSourceState() = default;

// ISpatialInteractionSourceState
bool WMRInputSourceState::TryGetPointerPose(
    const WMRCoordinateSystem* origin,
    WMRPointerPose* pointer_pose) const {
  ComPtr<ISpatialPointerPose> pointer_pose_wmr;
  HRESULT hr =
      source_state_->TryGetPointerPose(origin->GetRawPtr(), &pointer_pose_wmr);

  if (SUCCEEDED(hr) && pointer_pose_wmr) {
    *pointer_pose = WMRPointerPose(pointer_pose_wmr);
    return true;
  }

  return false;
}

WMRInputSource WMRInputSourceState::GetSource() const {
  ComPtr<ISpatialInteractionSource> source;
  HRESULT hr = source_state_->get_Source(&source);
  DCHECK(SUCCEEDED(hr));
  return WMRInputSource(source);
}

bool WMRInputSourceState::IsGrasped() const {
  boolean val;
  HRESULT hr = source_state2_->get_IsGrasped(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRInputSourceState::IsSelectPressed() const {
  boolean val;
  HRESULT hr = source_state2_->get_IsSelectPressed(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

double WMRInputSourceState::SelectPressedValue() const {
  DOUBLE val;
  HRESULT hr = source_state2_->get_SelectPressedValue(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRInputSourceState::SupportsControllerProperties() const {
  return controller_properties_ != nullptr;
}

bool WMRInputSourceState::IsThumbstickPressed() const {
  DCHECK(SupportsControllerProperties());
  boolean val;
  HRESULT hr = controller_properties_->get_IsThumbstickPressed(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRInputSourceState::IsTouchpadPressed() const {
  DCHECK(SupportsControllerProperties());
  boolean val;
  HRESULT hr = controller_properties_->get_IsTouchpadPressed(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRInputSourceState::IsTouchpadTouched() const {
  DCHECK(SupportsControllerProperties());
  boolean val;
  HRESULT hr = controller_properties_->get_IsTouchpadTouched(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

double WMRInputSourceState::ThumbstickX() const {
  DCHECK(SupportsControllerProperties());
  DOUBLE val;
  HRESULT hr = controller_properties_->get_ThumbstickX(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

double WMRInputSourceState::ThumbstickY() const {
  DCHECK(SupportsControllerProperties());
  DOUBLE val;
  HRESULT hr = controller_properties_->get_ThumbstickY(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

double WMRInputSourceState::TouchpadX() const {
  DCHECK(SupportsControllerProperties());
  DOUBLE val;
  HRESULT hr = controller_properties_->get_TouchpadX(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

double WMRInputSourceState::TouchpadY() const {
  DCHECK(SupportsControllerProperties());
  DOUBLE val;
  HRESULT hr = controller_properties_->get_TouchpadY(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRInputSourceState::TryGetLocation(const WMRCoordinateSystem* origin,
                                         WMRInputLocation* location) const {
  DCHECK(location);
  ComPtr<ISpatialInteractionSourceLocation> location_wmr;
  if (FAILED(properties_->TryGetLocation(origin->GetRawPtr(), &location_wmr)) ||
      !location_wmr)
    return false;

  *location = WMRInputLocation(location_wmr);
  return true;
}

}  // namespace device
