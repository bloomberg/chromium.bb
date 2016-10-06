// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/adapter.h"
#include "mojo/public/cpp/bindings/string.h"
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

void Adapter::GetDevices(const GetDevicesCallback& callback) {
  std::vector<mojom::DeviceInfoPtr> devices;

  for (const device::BluetoothDevice* device : adapter_->GetDevices()) {
    mojom::DeviceInfoPtr device_info = ConstructDeviceInfoStruct(device);
    devices.push_back(std::move(device_info));
  }

  callback.Run(std::move(devices));
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

// static
mojom::DeviceInfoPtr Adapter::ConstructDeviceInfoStruct(
    const device::BluetoothDevice* device) {
  mojom::DeviceInfoPtr device_info = mojom::DeviceInfo::New();

  device_info->name = device->GetName();
  device_info->name_for_display =
      base::UTF16ToUTF8(device->GetNameForDisplay());
  device_info->id = device->GetIdentifier();
  device_info->address = device->GetAddress();

  return device_info;
}

}  // namespace bluetooth
