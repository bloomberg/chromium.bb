// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_base.h"

#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_display_impl.h"

namespace device {

unsigned int VRDeviceBase::next_id_ = 1;

VRDeviceBase::VRDeviceBase() : id_(next_id_) {
  // Prevent wraparound. Devices with this ID will be treated as invalid.
  if (next_id_ != VR_DEVICE_LAST_ID)
    next_id_++;
}

VRDeviceBase::~VRDeviceBase() = default;

unsigned int VRDeviceBase::GetId() const {
  return id_;
}

void VRDeviceBase::PauseTracking() {}

void VRDeviceBase::ResumeTracking() {}

void VRDeviceBase::Blur() {
  for (VRDisplayImpl* display : displays_)
    display->OnBlur();
}

void VRDeviceBase::Focus() {
  for (VRDisplayImpl* display : displays_)
    display->OnFocus();
}

void VRDeviceBase::OnExitPresent() {
  if (!presenting_display_)
    return;
  auto it = displays_.find(presenting_display_);
  CHECK(it != displays_.end());
  (*it)->OnExitPresent();
  SetPresentingDisplay(nullptr);
}

mojom::VRDisplayInfoPtr VRDeviceBase::GetVRDisplayInfo() {
  DCHECK(display_info_);
  return display_info_.Clone();
}

void VRDeviceBase::RequestPresent(
    VRDisplayImpl* display,
    mojom::VRSubmitFrameClientPtr submit_client,
    mojom::VRPresentationProviderRequest request,
    mojom::VRDisplayHost::RequestPresentCallback callback) {
  std::move(callback).Run(false);
}

void VRDeviceBase::ExitPresent() {
  NOTREACHED();
}

void VRDeviceBase::GetPose(
    mojom::VRMagicWindowProvider::GetPoseCallback callback) {
  std::move(callback).Run(nullptr);
}

void VRDeviceBase::AddDisplay(VRDisplayImpl* display) {
  displays_.insert(display);
}

void VRDeviceBase::RemoveDisplay(VRDisplayImpl* display) {
  if (CheckPresentingDisplay(display))
    ExitPresent();
  displays_.erase(display);
  if (listening_for_activate_diplay_ == display) {
    listening_for_activate_diplay_ = nullptr;
    OnListeningForActivate(false);
  }
  if (last_listening_for_activate_diplay_ == display)
    last_listening_for_activate_diplay_ = nullptr;
}

bool VRDeviceBase::IsAccessAllowed(VRDisplayImpl* display) {
  return (!presenting_display_ || presenting_display_ == display);
}

bool VRDeviceBase::CheckPresentingDisplay(VRDisplayImpl* display) {
  return (presenting_display_ && presenting_display_ == display);
}

void VRDeviceBase::OnListeningForActivateChanged(VRDisplayImpl* display) {
  UpdateListeningForActivate(display);
}

void VRDeviceBase::OnFrameFocusChanged(VRDisplayImpl* display) {
  UpdateListeningForActivate(display);
}

void VRDeviceBase::SetPresentingDisplay(VRDisplayImpl* display) {
  presenting_display_ = display;
}

void VRDeviceBase::SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info) {
  DCHECK(display_info);
  DCHECK(display_info->index == id_);
  bool initialized = !!display_info_;
  display_info_ = std::move(display_info);

  // Don't notify when the VRDisplayInfo is initially set.
  if (!initialized)
    return;
  for (VRDisplayImpl* display : displays_)
    display->OnChanged(display_info_.Clone());
}

void VRDeviceBase::OnActivate(mojom::VRDisplayEventReason reason,
                              base::Callback<void(bool)> on_handled) {
  if (listening_for_activate_diplay_) {
    listening_for_activate_diplay_->OnActivate(reason, std::move(on_handled));
  } else if (last_listening_for_activate_diplay_ &&
             last_listening_for_activate_diplay_->InFocusedFrame()) {
    last_listening_for_activate_diplay_->OnActivate(reason,
                                                    std::move(on_handled));
  } else {
    std::move(on_handled).Run(true /* will_not_present */);
  }
}

void VRDeviceBase::OnListeningForActivate(bool listening) {}

void VRDeviceBase::UpdateListeningForActivate(VRDisplayImpl* display) {
  if (display->ListeningForActivate() && display->InFocusedFrame()) {
    bool was_listening = !!listening_for_activate_diplay_;
    listening_for_activate_diplay_ = display;
    if (!was_listening)
      OnListeningForActivate(true);
  } else if (listening_for_activate_diplay_ == display) {
    last_listening_for_activate_diplay_ = listening_for_activate_diplay_;
    listening_for_activate_diplay_ = nullptr;
    OnListeningForActivate(false);
  }
}

}  // namespace device
