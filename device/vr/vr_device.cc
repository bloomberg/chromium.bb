// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_display_impl.h"

namespace device {

unsigned int VRDevice::next_id_ = 1;

VRDevice::VRDevice()
    : presenting_display_(nullptr), id_(next_id_), weak_ptr_factory_(this) {
  // Prevent wraparound. Devices with this ID will be treated as invalid.
  if (next_id_ != VR_DEVICE_LAST_ID)
    next_id_++;
}

VRDevice::~VRDevice() {}

void VRDevice::AddDisplay(VRDisplayImpl* display) {
  displays_.insert(display);
}

void VRDevice::RemoveDisplay(VRDisplayImpl* display) {
  if (CheckPresentingDisplay(display))
    ExitPresent();
  displays_.erase(display);
}

bool VRDevice::IsAccessAllowed(VRDisplayImpl* display) {
  return (!presenting_display_ || presenting_display_ == display);
}

bool VRDevice::CheckPresentingDisplay(VRDisplayImpl* display) {
  return (presenting_display_ && presenting_display_ == display);
}

void VRDevice::OnChanged() {
  base::Callback<void(mojom::VRDisplayInfoPtr)> callback = base::Bind(
      &VRDevice::OnVRDisplayInfoCreated, weak_ptr_factory_.GetWeakPtr());
  CreateVRDisplayInfo(callback);
}

void VRDevice::OnVRDisplayInfoCreated(mojom::VRDisplayInfoPtr vr_device_info) {
  if (vr_device_info.is_null())
    return;
  for (auto* display : displays_)
    display->OnChanged(vr_device_info.Clone());
}

void VRDevice::OnExitPresent() {
  if (!presenting_display_)
    return;
  auto it = displays_.find(presenting_display_);
  CHECK(it != displays_.end());
  (*it)->OnExitPresent();
  SetPresentingDisplay(nullptr);
}

void VRDevice::OnBlur() {
  for (auto* display : displays_)
    display->OnBlur();
}

void VRDevice::OnFocus() {
  for (auto* display : displays_)
    display->OnFocus();
}

void VRDevice::OnActivate(mojom::VRDisplayEventReason reason,
                          const base::Callback<void(bool)>& on_handled) {
  for (auto* display : displays_)
    display->OnActivate(reason, on_handled);
}

void VRDevice::OnDeactivate(mojom::VRDisplayEventReason reason) {
  for (auto* display : displays_)
    display->OnDeactivate(reason);
}

void VRDevice::SetPresentingDisplay(VRDisplayImpl* display) {
  presenting_display_ = display;
}

}  // namespace device
