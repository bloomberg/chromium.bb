// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_service_impl.h"

namespace device {

VRDisplayImpl::VRDisplayImpl(device::VRDevice* device,
                             VRServiceImpl* service,
                             mojom::VRServiceClient* service_client,
                             mojom::VRDisplayInfoPtr display_info)
    : binding_(this),
      device_(device),
      service_(service),
      weak_ptr_factory_(this) {
  device_->AddDisplay(this);
  service_client->OnDisplayConnected(binding_.CreateInterfacePtrAndBind(),
                                     mojo::MakeRequest(&client_),
                                     std::move(display_info));
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

void VRDisplayImpl::OnActivate(mojom::VRDisplayEventReason reason) {
  client_->OnActivate(reason);
}

void VRDisplayImpl::OnDeactivate(mojom::VRDisplayEventReason reason) {
  client_->OnDeactivate(reason);
}

void VRDisplayImpl::ResetPose() {
  if (!device_->IsAccessAllowed(this))
    return;

  device_->ResetPose();
}

void VRDisplayImpl::RequestPresent(bool secure_origin,
                                   mojom::VRSubmitFrameClientPtr submit_client,
                                   const RequestPresentCallback& callback) {
  if (!device_->IsAccessAllowed(this)) {
    callback.Run(false);
    return;
  }

  device_->RequestPresent(
      std::move(submit_client),
      base::Bind(&VRDisplayImpl::RequestPresentResult,
                 weak_ptr_factory_.GetWeakPtr(), callback, secure_origin));
}

void VRDisplayImpl::RequestPresentResult(const RequestPresentCallback& callback,
                                         bool secure_origin,
                                         bool success) {
  if (success) {
    device_->SetPresentingDisplay(this);
    device_->SetSecureOrigin(secure_origin);
  }
  callback.Run(success);
}

void VRDisplayImpl::ExitPresent() {
  if (device_->CheckPresentingDisplay(this))
    device_->ExitPresent();
}

void VRDisplayImpl::SubmitFrame(int16_t frame_index,
                                const gpu::MailboxHolder& mailbox) {
  if (!device_->CheckPresentingDisplay(this))
    return;
  device_->SubmitFrame(frame_index, mailbox);
}

void VRDisplayImpl::UpdateLayerBounds(int16_t frame_index,
                                      mojom::VRLayerBoundsPtr left_bounds,
                                      mojom::VRLayerBoundsPtr right_bounds,
                                      int16_t source_width,
                                      int16_t source_height) {
  if (!device_->IsAccessAllowed(this))
    return;

  device_->UpdateLayerBounds(frame_index, std::move(left_bounds),
                             std::move(right_bounds), source_width,
                             source_height);
}

void VRDisplayImpl::GetVRVSyncProvider(mojom::VRVSyncProviderRequest request) {
  if (!device_->IsAccessAllowed(this)) {
    return;
  }
  device_->GetVRVSyncProvider(std::move(request));
}
}
