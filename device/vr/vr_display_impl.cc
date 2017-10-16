// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_display_impl.h"

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device_base.h"

namespace device {

VRDisplayImpl::VRDisplayImpl(VRDevice* device,
                             mojom::VRServiceClient* service_client,
                             mojom::VRDisplayInfoPtr display_info,
                             mojom::VRDisplayHostPtr display_host,
                             bool in_focused_frame)
    : binding_(this),
      device_(static_cast<VRDeviceBase*>(device)),
      in_focused_frame_(in_focused_frame) {
  device_->AddDisplay(this);
  mojom::VRMagicWindowProviderPtr magic_window_provider;
  binding_.Bind(mojo::MakeRequest(&magic_window_provider));
  service_client->OnDisplayConnected(
      std::move(magic_window_provider), std::move(display_host),
      mojo::MakeRequest(&client_), std::move(display_info));
}

VRDisplayImpl::~VRDisplayImpl() {
  device_->RemoveDisplay(this);
}

void VRDisplayImpl::OnChanged(mojom::VRDisplayInfoPtr vr_device_info) {
  client_->OnChanged(std::move(vr_device_info));
}

void VRDisplayImpl::OnExitPresent() {
  client_->OnExitPresent();
}

void VRDisplayImpl::OnBlur() {
  client_->OnBlur();
}

void VRDisplayImpl::OnFocus() {
  client_->OnFocus();
}

void VRDisplayImpl::OnActivate(mojom::VRDisplayEventReason reason,
                               base::Callback<void(bool)> on_handled) {
  client_->OnActivate(reason, std::move(on_handled));
}

void VRDisplayImpl::OnDeactivate(mojom::VRDisplayEventReason reason) {
  client_->OnDeactivate(reason);
}

void VRDisplayImpl::RequestPresent(
    mojom::VRSubmitFrameClientPtr submit_client,
    mojom::VRPresentationProviderRequest request,
    mojom::VRDisplayHost::RequestPresentCallback callback) {
  if (!device_->IsAccessAllowed(this) || !InFocusedFrame()) {
    std::move(callback).Run(false);
    return;
  }

  device_->RequestPresent(this, std::move(submit_client), std::move(request),
                          std::move(callback));
}

void VRDisplayImpl::ExitPresent() {
  if (device_->CheckPresentingDisplay(this))
    device_->ExitPresent();
}

void VRDisplayImpl::GetPose(GetPoseCallback callback) {
  if (!device_->IsAccessAllowed(this)) {
    std::move(callback).Run(nullptr);
    return;
  }
  device_->GetPose(std::move(callback));
}

void VRDisplayImpl::SetListeningForActivate(bool listening) {
  listening_for_activate_ = listening;
  device_->OnListeningForActivateChanged(this);
}

void VRDisplayImpl::SetInFocusedFrame(bool in_focused_frame) {
  in_focused_frame_ = in_focused_frame;
  device_->OnFrameFocusChanged(this);
}

bool VRDisplayImpl::ListeningForActivate() {
  return listening_for_activate_;
}

bool VRDisplayImpl::InFocusedFrame() {
  return in_focused_frame_;
}

}  // namespace device
