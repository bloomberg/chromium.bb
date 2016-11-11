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
  callback.Run(device_->GetPose(service_.get()));
}

void VRDisplayImpl::ResetPose() {
  device_->ResetPose(service_.get());
}

void VRDisplayImpl::RequestPresent(bool secureOrigin,
                                   const RequestPresentCallback& callback) {
  callback.Run(device_->RequestPresent(service_.get(), secureOrigin));
}

void VRDisplayImpl::ExitPresent() {
  device_->ExitPresent(service_.get());
}

void VRDisplayImpl::SubmitFrame(mojom::VRPosePtr pose) {
  device_->SubmitFrame(service_.get(), std::move(pose));
}

void VRDisplayImpl::UpdateLayerBounds(mojom::VRLayerBoundsPtr leftBounds,
                                      mojom::VRLayerBoundsPtr rightBounds) {
  device_->UpdateLayerBounds(service_.get(), std::move(leftBounds),
                             std::move(rightBounds));
}
}
