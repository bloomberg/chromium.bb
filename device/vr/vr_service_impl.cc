// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_manager.h"

namespace device {

VRServiceImpl::VRServiceImpl() {}

VRServiceImpl::~VRServiceImpl() {
  RemoveFromDeviceManager();
}

void VRServiceImpl::BindRequest(mojo::InterfaceRequest<VRService> request) {
  VRServiceImpl* service = new VRServiceImpl();
  service->Bind(std::move(request));
}

void VRServiceImpl::Bind(mojo::InterfaceRequest<VRService> request) {
  binding_.reset(new mojo::Binding<VRService>(this, std::move(request)));
  binding_->set_connection_error_handler(base::Bind(
      &VRServiceImpl::RemoveFromDeviceManager, base::Unretained(this)));
}

void VRServiceImpl::RemoveFromDeviceManager() {
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  device_manager->RemoveService(this);
}

void VRServiceImpl::SetClient(VRServiceClientPtr client) {
  DCHECK(!client_.get());

  client_ = std::move(client);
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  device_manager->AddService(this);
}

void VRServiceImpl::GetDisplays(const GetDisplaysCallback& callback) {
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  callback.Run(device_manager->GetVRDevices());
}

void VRServiceImpl::GetPose(uint32_t index, const GetPoseCallback& callback) {
  VRDevice* device = VRDeviceManager::GetAllowedDevice(this, index);

  if (device) {
    callback.Run(device->GetPose());
  } else {
    callback.Run(nullptr);
  }
}

void VRServiceImpl::ResetPose(uint32_t index) {
  VRDevice* device = VRDeviceManager::GetAllowedDevice(this, index);
  if (device)
    device->ResetPose();
}

void VRServiceImpl::RequestPresent(uint32_t index,
                                   bool secureOrigin,
                                   const RequestPresentCallback& callback) {
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  callback.Run(device_manager->RequestPresent(this, index, secureOrigin));
}

void VRServiceImpl::ExitPresent(uint32_t index) {
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  device_manager->ExitPresent(this, index);
}

void VRServiceImpl::SubmitFrame(uint32_t index, VRPosePtr pose) {
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  device_manager->SubmitFrame(this, index, std::move(pose));
}

void VRServiceImpl::UpdateLayerBounds(uint32_t index,
                                      VRLayerBoundsPtr leftBounds,
                                      VRLayerBoundsPtr rightBounds) {
  VRDevice* device = VRDeviceManager::GetAllowedDevice(this, index);
  if (device)
    device->UpdateLayerBounds(std::move(leftBounds), std::move(rightBounds));
}

}  // namespace device
