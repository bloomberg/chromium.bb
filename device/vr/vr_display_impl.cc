// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_display_impl.h"

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_manager.h"
#include "device/vr/vr_service_impl.h"

namespace device {

VRDisplayImpl::VRDisplayImpl(device::VRDevice* device,
                             int render_frame_process_id,
                             int render_frame_routing_id,
                             mojom::VRServiceClient* service_client,
                             mojom::VRDisplayInfoPtr display_info)
    : binding_(this),
      device_(device),
      render_frame_process_id_(render_frame_process_id),
      render_frame_routing_id_(render_frame_routing_id),
      weak_ptr_factory_(this) {
  device_->AddDisplay(this);
  mojom::VRDisplayPtr display;
  binding_.Bind(mojo::MakeRequest(&display));
  service_client->OnDisplayConnected(
      std::move(display), mojo::MakeRequest(&client_), std::move(display_info));
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
                               const base::Callback<void(bool)>& on_handled) {
  client_->OnActivate(reason, on_handled);
}

void VRDisplayImpl::OnDeactivate(mojom::VRDisplayEventReason reason) {
  client_->OnDeactivate(reason);
}

void VRDisplayImpl::RequestPresent(bool secure_origin,
                                   mojom::VRSubmitFrameClientPtr submit_client,
                                   mojom::VRPresentationProviderRequest request,
                                   RequestPresentCallback callback) {
  if (!device_->IsAccessAllowed(this)) {
    std::move(callback).Run(false);
    return;
  }

  device_->RequestPresent(std::move(submit_client), std::move(request),
                          base::Bind(&VRDisplayImpl::RequestPresentResult,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     base::Passed(&callback), secure_origin));
}

void VRDisplayImpl::RequestPresentResult(RequestPresentCallback callback,
                                         bool secure_origin,
                                         bool success) {
  if (success) {
    device_->SetPresentingDisplay(this);
    device_->SetSecureOrigin(secure_origin);
  }
  std::move(callback).Run(success);
}

void VRDisplayImpl::ExitPresent() {
  if (device_->CheckPresentingDisplay(this))
    device_->ExitPresent();
}

void VRDisplayImpl::GetNextMagicWindowPose(
    GetNextMagicWindowPoseCallback callback) {
  if (!device_->IsAccessAllowed(this)) {
    std::move(callback).Run(nullptr);
    return;
  }
  device_->GetNextMagicWindowPose(this, std::move(callback));
}

void VRDisplayImpl::SetListeningForActivate(bool listening) {
  listening_for_activate_ = listening;
  device_->OnListeningForActivateChanged(this);
}

}  // namespace device
