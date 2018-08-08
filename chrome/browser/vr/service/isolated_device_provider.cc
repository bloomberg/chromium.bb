// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/isolated_device_provider.h"
#include "content/public/common/service_manager_connection.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/isolated_gamepad_data_fetcher.h"
#include "services/service_manager/public/cpp/connector.h"

namespace vr {

void IsolatedVRDeviceProvider::Initialize(
    base::RepeatingCallback<void(device::mojom::XRDeviceId,
                                 device::mojom::VRDisplayInfoPtr,
                                 device::mojom::XRRuntimePtr)>
        add_device_callback,
    base::RepeatingCallback<void(device::mojom::XRDeviceId)>
        remove_device_callback,
    base::OnceClosure initialization_complete) {
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(
      device::mojom::kVrIsolatedServiceName,
      mojo::MakeRequest(&device_provider_));

  device::mojom::IsolatedXRRuntimeProviderClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  device_provider_->RequestDevices(std::move(client));

  add_device_callback_ = std::move(add_device_callback);
  remove_device_callback_ = std::move(remove_device_callback);
  initialization_complete_ = std::move(initialization_complete);
}

bool IsolatedVRDeviceProvider::Initialized() {
  return initialized_;
}

void IsolatedVRDeviceProvider::OnDeviceAdded(
    device::mojom::XRRuntimePtr device,
    device::mojom::IsolatedXRGamepadProviderFactoryPtr gamepad_factory,
    device::mojom::VRDisplayInfoPtr display_info) {
  device::mojom::XRDeviceId id = display_info->id;
  add_device_callback_.Run(id, std::move(display_info), std::move(device));

#if BUILDFLAG(ENABLE_OPENVR) || BUILDFLAG(ENABLE_OCULUS_VR)
  registered_gamepads_.insert(id);
  device::IsolatedGamepadDataFetcher::Factory::AddGamepad(
      id, std::move(gamepad_factory));
#endif
}

void IsolatedVRDeviceProvider::OnDeviceRemoved(device::mojom::XRDeviceId id) {
  remove_device_callback_.Run(id);

#if BUILDFLAG(ENABLE_OPENVR) || BUILDFLAG(ENABLE_OCULUS_VR)
  device::IsolatedGamepadDataFetcher::Factory::RemoveGamepad(id);
#endif
}

void IsolatedVRDeviceProvider::OnDevicesEnumerated() {
  initialized_ = true;
  std::move(initialization_complete_).Run();
}

IsolatedVRDeviceProvider::IsolatedVRDeviceProvider() : binding_(this) {}

IsolatedVRDeviceProvider::~IsolatedVRDeviceProvider() {
#if BUILDFLAG(ENABLE_OPENVR) || BUILDFLAG(ENABLE_OCULUS_VR)
  for (auto gamepad_id : registered_gamepads_) {
    device::IsolatedGamepadDataFetcher::Factory::RemoveGamepad(gamepad_id);
  }
#endif
}

}  // namespace vr
