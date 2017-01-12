// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "device/bluetooth/adapter.h"
#include "device/bluetooth/device.h"
#include "device/bluetooth/discovery_session.h"
#include "device/bluetooth/public/interfaces/connect_result_type_converter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace bluetooth {

Adapter::Adapter(scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(std::move(adapter)), client_(nullptr), weak_ptr_factory_(this) {
  adapter_->AddObserver(this);
}

Adapter::~Adapter() {
  adapter_->RemoveObserver(this);
  adapter_ = nullptr;
}

void Adapter::ConnectToDevice(const std::string& address,
                              const ConnectToDeviceCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(address);

  if (!device) {
    callback.Run(mojom::ConnectResult::DEVICE_NO_LONGER_IN_RANGE,
                 nullptr /* device */);
    return;
  }

  device->CreateGattConnection(
      base::Bind(&Adapter::OnGattConnected, weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&Adapter::OnConnectError, weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void Adapter::GetDevices(const GetDevicesCallback& callback) {
  std::vector<mojom::DeviceInfoPtr> devices;

  for (const device::BluetoothDevice* device : adapter_->GetDevices()) {
    mojom::DeviceInfoPtr device_info =
        Device::ConstructDeviceInfoStruct(device);
    devices.push_back(std::move(device_info));
  }

  callback.Run(std::move(devices));
}

void Adapter::GetInfo(const GetInfoCallback& callback) {
  mojom::AdapterInfoPtr adapter_info = mojom::AdapterInfo::New();
  adapter_info->address = adapter_->GetAddress();
  adapter_info->name = adapter_->GetName();
  adapter_info->initialized = adapter_->IsInitialized();
  adapter_info->present = adapter_->IsPresent();
  adapter_info->powered = adapter_->IsPowered();
  adapter_info->discoverable = adapter_->IsDiscoverable();
  adapter_info->discovering = adapter_->IsDiscovering();
  callback.Run(std::move(adapter_info));
}

void Adapter::SetClient(mojom::AdapterClientPtr client) {
  client_ = std::move(client);
}

void Adapter::StartDiscoverySession(
    const StartDiscoverySessionCallback& callback) {
  adapter_->StartDiscoverySession(
      base::Bind(&Adapter::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&Adapter::OnDiscoverySessionError,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void Adapter::AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                    bool present) {
  if (client_)
    client_->PresentChanged(present);
}

void Adapter::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                    bool powered) {
  if (client_)
    client_->PoweredChanged(powered);
}

void Adapter::AdapterDiscoverableChanged(device::BluetoothAdapter* adapter,
                                         bool discoverable) {
  if (client_)
    client_->DiscoverableChanged(discoverable);
}

void Adapter::AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                        bool discovering) {
  if (client_)
    client_->DiscoveringChanged(discovering);
}

void Adapter::DeviceAdded(device::BluetoothAdapter* adapter,
                          device::BluetoothDevice* device) {
  if (client_) {
    auto device_info = Device::ConstructDeviceInfoStruct(device);
    client_->DeviceAdded(std::move(device_info));
  }
}

void Adapter::DeviceChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device) {
  if (client_) {
    auto device_info = Device::ConstructDeviceInfoStruct(device);
    client_->DeviceChanged(std::move(device_info));
  }
}

void Adapter::DeviceRemoved(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device) {
  if (client_) {
    auto device_info = Device::ConstructDeviceInfoStruct(device);
    client_->DeviceRemoved(std::move(device_info));
  }
}

void Adapter::OnGattConnected(
    const ConnectToDeviceCallback& callback,
    std::unique_ptr<device::BluetoothGattConnection> connection) {
  mojom::DevicePtr device_ptr;
  Device::Create(adapter_, std::move(connection),
                 mojo::MakeRequest(&device_ptr));
  callback.Run(mojom::ConnectResult::SUCCESS, std::move(device_ptr));
}

void Adapter::OnConnectError(
    const ConnectToDeviceCallback& callback,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  callback.Run(mojo::ConvertTo<mojom::ConnectResult>(error_code),
               nullptr /* Device */);
}

void Adapter::OnStartDiscoverySession(
    const StartDiscoverySessionCallback& callback,
    std::unique_ptr<device::BluetoothDiscoverySession> session) {
  mojom::DiscoverySessionPtr session_ptr;
  mojo::MakeStrongBinding(
      base::MakeUnique<DiscoverySession>(std::move(session)),
      mojo::MakeRequest(&session_ptr));
  callback.Run(std::move(session_ptr));
}

void Adapter::OnDiscoverySessionError(
    const StartDiscoverySessionCallback& callback) {
  callback.Run(nullptr /* session */);
}

}  // namespace bluetooth
