// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/vr_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/service/vr_device_manager.h"
#include "chrome/browser/vr/service/vr_display_host.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_display_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace vr {

VRServiceImpl::VRServiceImpl(int render_frame_process_id,
                             int render_frame_routing_id)
    : render_frame_process_id_(render_frame_process_id),
      render_frame_routing_id_(render_frame_routing_id),
      weak_ptr_factory_(this) {
  // TODO(crbug/701027): make sure that client_ is never null by initializing it
  // in the constructor.
}

VRServiceImpl::~VRServiceImpl() {
  // Destroy VRDisplay before calling RemoveService below. RemoveService might
  // implicitly trigger destory VRDevice which VRDisplay needs to access in its
  // dtor.
  displays_.clear();
  VRDeviceManager::GetInstance()->RemoveService(this);
}

void VRServiceImpl::Create(int render_frame_process_id,
                           int render_frame_routing_id,
                           device::mojom::VRServiceRequest request) {
  mojo::MakeStrongBinding(std::make_unique<VRServiceImpl>(
                              render_frame_process_id, render_frame_routing_id),
                          std::move(request));
}

void VRServiceImpl::SetClient(device::mojom::VRServiceClientPtr service_client,
                              SetClientCallback callback) {
  DCHECK(!client_.get());
  client_ = std::move(service_client);

  // Once a client has been connected AddService will force any VRDisplays to
  // send ConnectDevice to it so that it's populated with the currently active
  // displays. Thereafter it will stay up to date by virtue of listening for new
  // connected events.
  VRDeviceManager::GetInstance()->AddService(this);

  // After displays are added, run the callback to let the VRController know
  // initialization is complete.
  std::move(callback).Run();
}

// Creates a VRDisplayImpl unique to this service so that the associated page
// can communicate with the VRDevice.
void VRServiceImpl::ConnectDevice(device::VRDevice* device) {
  // Client should always be set as this is called through SetClient.
  DCHECK(client_);
  DCHECK(displays_.count(device) == 0);
  device::mojom::VRDisplayInfoPtr display_info = device->GetVRDisplayInfo();
  DCHECK(display_info);
  if (!display_info)
    return;
  displays_[device] = std::make_unique<VRDisplayHost>(
      device, render_frame_process_id_, render_frame_routing_id_, client_.get(),
      std::move(display_info));
}

void VRServiceImpl::SetListeningForActivate(bool listening) {
  for (const auto& display : displays_) {
    display.second->SetListeningForActivate(listening);
  }
}

}  // namespace vr
