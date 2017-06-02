// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_manager.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

VRServiceImpl::VRServiceImpl()
    : listening_for_activate_(false),
      connected_devices_(0),
      weak_ptr_factory_(this) {}

VRServiceImpl::~VRServiceImpl() {
  // Destroy VRDisplay before calling RemoveService below. RemoveService might
  // implicitly trigger destory VRDevice which VRDisplay needs to access in its
  // dtor.
  displays_.clear();
  VRDeviceManager::GetInstance()->RemoveService(this);
}

void VRServiceImpl::Create(const service_manager::BindSourceInfo& source_info,
                           mojom::VRServiceRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<VRServiceImpl>(),
                          std::move(request));
}

void VRServiceImpl::SetClient(mojom::VRServiceClientPtr service_client,
                              SetClientCallback callback) {
  DCHECK(!client_.get());
  client_ = std::move(service_client);
  // Once a client has been connected AddService will force any VRDisplays to
  // send ConnectDevice to it so that it's populated with the currently active
  // displays. Thereafter it will stay up to date by virtue of listening for new
  // connected events.
  VRDeviceManager::GetInstance()->AddService(this);
  std::move(callback).Run(connected_devices_);
}

void VRServiceImpl::ConnectDevice(VRDevice* device) {
  DCHECK(displays_.count(device) == 0);
  base::Callback<void(mojom::VRDisplayInfoPtr)> on_created =
      base::Bind(&VRServiceImpl::OnVRDisplayInfoCreated,
                 weak_ptr_factory_.GetWeakPtr(), device);
  device->CreateVRDisplayInfo(on_created);
}

void VRServiceImpl::SetListeningForActivate(bool listening) {
  listening_for_activate_ = listening;
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  device_manager->ListeningForActivateChanged(listening, this);
}

// Creates a VRDisplayPtr unique to this service so that the associated page can
// communicate with the VRDevice.
void VRServiceImpl::OnVRDisplayInfoCreated(
    VRDevice* device,
    mojom::VRDisplayInfoPtr display_info) {
  // TODO(crbug/701027): make sure that client_ is never null by initializing it
  // in the constructor.
  if (!client_) {
    DLOG(ERROR) << "Cannot create VR display because connection to render "
                   "process is not established";
    return;
  }

  if (!display_info) {
    // If we get passed a null display info it means the device does not exist.
    // This can happen for example if VR services are not installed. We will not
    // instantiate a display in this case and don't count it as connected.
    return;
  }
  displays_[device] = base::MakeUnique<VRDisplayImpl>(
      device, this, client_.get(), std::move(display_info));
  connected_devices_++;
}

VRDisplayImpl* VRServiceImpl::GetVRDisplayImplForTesting(VRDevice* device) {
  auto it = displays_.find(device);
  return (it == displays_.end()) ? nullptr : it->second.get();
}

}  // namespace device
