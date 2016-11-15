// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_service_impl.h"

namespace device {

VRDisplayImpl::VRDisplayImpl(device::VRDevice* device, VRServiceImpl* service)
    : binding_(this), device_(device), service_(service) {
  mojom::VRDisplayInfoPtr display_info = device->GetVRDevice();
  service->client()->OnDisplayConnected(binding_.CreateInterfacePtrAndBind(),
                                        mojo::GetProxy(&client_),
                                        std::move(display_info));
}

VRDisplayImpl::~VRDisplayImpl() {}

void VRDisplayImpl::GetPose(const GetPoseCallback& callback) {
  if (!device_->IsAccessAllowed(service_.get())) {
    callback.Run(nullptr);
    return;
  }

  callback.Run(device_->GetPose());
}

void VRDisplayImpl::ResetPose() {
  if (!device_->IsAccessAllowed(service_.get()))
    return;

  device_->ResetPose();
}

void VRDisplayImpl::RequestPresent(bool secureOrigin,
                                   const RequestPresentCallback& callback) {
  if (!device_->IsAccessAllowed(service_.get())) {
    callback.Run(false);
    return;
  }

  bool success = device_->RequestPresent(secureOrigin);
  if (success) {
    device_->SetPresentingService(service_.get());
  }
  callback.Run(success);
}

void VRDisplayImpl::ExitPresent() {
  if (device_->IsPresentingService(service_.get()))
    device_->ExitPresent();
}

void VRDisplayImpl::SubmitFrame(mojom::VRPosePtr pose) {
  if (!device_->IsPresentingService(service_.get()))
    return;
  device_->SubmitFrame(std::move(pose));
}

void VRDisplayImpl::UpdateLayerBounds(mojom::VRLayerBoundsPtr leftBounds,
                                      mojom::VRLayerBoundsPtr rightBounds) {
  if (!device_->IsAccessAllowed(service_.get()))
    return;

  device_->UpdateLayerBounds(std::move(leftBounds), std::move(rightBounds));
}
}
