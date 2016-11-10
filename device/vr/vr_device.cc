// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_service_impl.h"

namespace device {

unsigned int VRDevice::next_id_ = 1;

VRDevice::VRDevice(VRDeviceProvider* provider)
    : presenting_service_(nullptr), provider_(provider), id_(next_id_) {
  // Prevent wraparound. Devices with this ID will be treated as invalid.
  if (next_id_ != VR_DEVICE_LAST_ID)
    next_id_++;
}

VRDevice::~VRDevice() {}

bool VRDevice::RequestPresent(VRServiceImpl* service, bool secure_origin) {
  return true;
};

void VRDevice::AddService(VRServiceImpl* service) {
  // Create a VRDisplayImpl for this service/device pair
  VRDisplayImpl* display_impl = service->GetVRDisplayImpl(this);
  displays_.insert(std::make_pair(service, display_impl));
}

void VRDevice::RemoveService(VRServiceImpl* service) {
  ExitPresent(service);
  displays_.erase(service);
}

bool VRDevice::IsAccessAllowed(VRServiceImpl* service) {
  return (!presenting_service_ || presenting_service_ == service);
}

bool VRDevice::IsPresentingService(VRServiceImpl* service) {
  return (presenting_service_ && presenting_service_ == service);
}

void VRDevice::OnDisplayChanged() {
  mojom::VRDisplayInfoPtr vr_device_info = GetVRDevice();
  if (vr_device_info.is_null())
    return;

  for (const auto& display : displays_) {
    mojom::VRDisplayClient* client = display.second->client();
    if (client)
      client->OnDisplayChanged(vr_device_info.Clone());
  }
}

void VRDevice::OnExitPresent(VRServiceImpl* service) {
  DisplayClientMap::iterator it = displays_.find(service);
  if (it != displays_.end()) {
    mojom::VRDisplayClient* client = it->second->client();
    if (client)
      client->OnExitPresent();
  }
}

}  // namespace device
