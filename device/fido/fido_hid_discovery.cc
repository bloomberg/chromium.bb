// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_hid_discovery.h"

#include <utility>

#include "device/fido/fido_hid_device.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

FidoHidDiscovery::FidoHidDiscovery(::service_manager::Connector* connector)
    : FidoDiscovery(FidoTransportProtocol::kUsbHumanInterfaceDevice),
      connector_(connector),
      binding_(this),
      weak_factory_(this) {
  // TODO(piperc@): Give this constant a name.
  filter_.SetUsagePage(0xf1d0);
}

FidoHidDiscovery::~FidoHidDiscovery() = default;

void FidoHidDiscovery::StartInternal() {
  DCHECK(connector_);
  connector_->BindInterface(device::mojom::kServiceName,
                            mojo::MakeRequest(&hid_manager_));
  device::mojom::HidManagerClientAssociatedPtrInfo client;
  binding_.Bind(mojo::MakeRequest(&client));

  hid_manager_->GetDevicesAndSetClient(
      std::move(client), base::BindOnce(&FidoHidDiscovery::OnGetDevices,
                                        weak_factory_.GetWeakPtr()));
}

void FidoHidDiscovery::DeviceAdded(
    device::mojom::HidDeviceInfoPtr device_info) {
  // Ignore non-U2F devices.
  if (filter_.Matches(*device_info)) {
    AddDevice(std::make_unique<FidoHidDevice>(std::move(device_info),
                                              hid_manager_.get()));
  }
}

void FidoHidDiscovery::DeviceRemoved(
    device::mojom::HidDeviceInfoPtr device_info) {
  // Ignore non-U2F devices.
  if (filter_.Matches(*device_info)) {
    RemoveDevice(FidoHidDevice::GetIdForDevice(*device_info));
  }
}

void FidoHidDiscovery::OnGetDevices(
    std::vector<device::mojom::HidDeviceInfoPtr> device_infos) {
  for (auto& device_info : device_infos)
    DeviceAdded(std::move(device_info));
  NotifyDiscoveryStarted(true);
}

}  // namespace device
