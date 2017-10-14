// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device.h"

#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_display_impl.h"

namespace device {

unsigned int VRDevice::next_id_ = 1;

VRDevice::VRDevice() : id_(next_id_) {
  // Prevent wraparound. Devices with this ID will be treated as invalid.
  if (next_id_ != VR_DEVICE_LAST_ID)
    next_id_++;
}

VRDevice::~VRDevice() {}

unsigned int VRDevice::id() const {
  return id_;
}

void VRDevice::AddDisplay(VRDisplayImpl* display) {
  displays_.insert(display);
}

void VRDevice::RemoveDisplay(VRDisplayImpl* display) {
  if (CheckPresentingDisplay(display))
    ExitPresent();
  displays_.erase(display);
  if (listening_for_activate_diplay_ == display)
    listening_for_activate_diplay_ = nullptr;
  if (last_listening_for_activate_diplay_ == display)
    last_listening_for_activate_diplay_ = nullptr;
}

bool VRDevice::IsAccessAllowed(VRDisplayImpl* display) {
  return (!presenting_display_ || presenting_display_ == display);
}

bool VRDevice::CheckPresentingDisplay(VRDisplayImpl* display) {
  return (presenting_display_ && presenting_display_ == display);
}

void VRDevice::OnChanged() {
  mojom::VRDisplayInfoPtr display_info = GetVRDisplayInfo();
  if (!display_info)
    return;
  for (VRDisplayImpl* display : displays_)
    display->OnChanged(display_info.Clone());
}

void VRDevice::OnExitPresent() {
  if (!presenting_display_)
    return;
  auto it = displays_.find(presenting_display_);
  CHECK(it != displays_.end());
  (*it)->OnExitPresent();
  SetPresentingDisplay(nullptr);
}

void VRDevice::OnActivate(mojom::VRDisplayEventReason reason,
                          const base::Callback<void(bool)>& on_handled) {
  if (listening_for_activate_diplay_) {
    listening_for_activate_diplay_->OnActivate(reason, on_handled);
  } else if (last_listening_for_activate_diplay_ &&
             last_listening_for_activate_diplay_->InFocusedFrame()) {
    last_listening_for_activate_diplay_->OnActivate(reason, on_handled);
  } else {
    std::move(on_handled).Run(true /* will_not_present */);
  }
}

void VRDevice::OnBlur() {
  for (VRDisplayImpl* display : displays_)
    display->OnBlur();
}

void VRDevice::OnFocus() {
  for (VRDisplayImpl* display : displays_)
    display->OnFocus();
}

void VRDevice::SetPresentingDisplay(VRDisplayImpl* display) {
  presenting_display_ = display;
}

void VRDevice::OnListeningForActivateChanged(VRDisplayImpl* display) {
  UpdateListeningForActivate(display);
}

void VRDevice::OnFrameFocusChanged(VRDisplayImpl* display) {
  UpdateListeningForActivate(display);
}

void VRDevice::UpdateListeningForActivate(VRDisplayImpl* display) {
  if (display->ListeningForActivate() && display->InFocusedFrame()) {
    listening_for_activate_diplay_ = display;
  } else if (listening_for_activate_diplay_ == display) {
    last_listening_for_activate_diplay_ = listening_for_activate_diplay_;
    listening_for_activate_diplay_ = nullptr;
  }
  OnListeningForActivateChanged(!!listening_for_activate_diplay_);
}

}  // namespace device
