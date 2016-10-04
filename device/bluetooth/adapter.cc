// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace bluetooth {

Adapter::Adapter() : client_(nullptr), weak_ptr_factory_(this) {}

Adapter::~Adapter() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = nullptr;
  }
}

// static
void Adapter::Create(mojom::AdapterRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<Adapter>(), std::move(request));
}

void Adapter::GetDevices(const GetDevicesCallback& callback) {
  if (!adapter_) {
    if (device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
      base::Closure c = base::Bind(&Adapter::GetDevicesImpl,
                                   weak_ptr_factory_.GetWeakPtr(), callback);

      device::BluetoothAdapterFactory::GetAdapter(base::Bind(
          &Adapter::OnGetAdapter, weak_ptr_factory_.GetWeakPtr(), c));
      return;
    }
    callback.Run(std::vector<mojom::DeviceInfoPtr>());
    return;
  }
  GetDevicesImpl(callback);
}

void Adapter::SetClient(mojom::AdapterClientPtr client) {
  client_ = std::move(client);
}

void Adapter::DeviceAdded(device::BluetoothAdapter* adapter,
                          device::BluetoothDevice* device) {
  if (client_) {
    auto device_info = ConstructDeviceInfoStruct(device);
    client_->DeviceAdded(std::move(device_info));
  }
}

void Adapter::DeviceRemoved(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device) {
  if (client_) {
    auto device_info = ConstructDeviceInfoStruct(device);
    client_->DeviceRemoved(std::move(device_info));
  }
}

mojom::DeviceInfoPtr Adapter::ConstructDeviceInfoStruct(
    const device::BluetoothDevice* device) const {
  mojom::DeviceInfoPtr device_info = mojom::DeviceInfo::New();

  device_info->name = device->GetName();
  device_info->name_for_display =
      base::UTF16ToUTF8(device->GetNameForDisplay());
  device_info->id = device->GetIdentifier();
  device_info->address = device->GetAddress();

  return device_info;
}

void Adapter::GetDevicesImpl(const GetDevicesCallback& callback) {
  std::vector<mojom::DeviceInfoPtr> devices;

  for (const device::BluetoothDevice* device : adapter_->GetDevices()) {
    mojom::DeviceInfoPtr device_info = ConstructDeviceInfoStruct(device);
    devices.push_back(std::move(device_info));
  }

  callback.Run(std::move(devices));
}

void Adapter::OnGetAdapter(const base::Closure& continuation,
                           scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter_) {
    VLOG(1) << "Adapter acquired";
    adapter_ = adapter;
    adapter_->AddObserver(this);
  }
  continuation.Run();
}

}  // namespace bluetooth
