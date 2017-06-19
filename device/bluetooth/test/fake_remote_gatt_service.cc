// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_remote_gatt_service.h"

#include <vector>

#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace bluetooth {

FakeRemoteGattService::FakeRemoteGattService(
    const std::string& service_id,
    const device::BluetoothUUID& service_uuid,
    bool is_primary,
    device::BluetoothDevice* device)
    : service_id_(service_id),
      service_uuid_(service_uuid),
      is_primary_(is_primary),
      device_(device) {}

FakeRemoteGattService::~FakeRemoteGattService() {}

std::string FakeRemoteGattService::GetIdentifier() const {
  return service_id_;
}

device::BluetoothUUID FakeRemoteGattService::GetUUID() const {
  return service_uuid_;
}

bool FakeRemoteGattService::IsPrimary() const {
  return is_primary_;
}

device::BluetoothDevice* FakeRemoteGattService::GetDevice() const {
  return device_;
}

std::vector<device::BluetoothRemoteGattCharacteristic*>
FakeRemoteGattService::GetCharacteristics() const {
  NOTREACHED();
  return std::vector<device::BluetoothRemoteGattCharacteristic*>();
}

std::vector<device::BluetoothRemoteGattService*>
FakeRemoteGattService::GetIncludedServices() const {
  NOTREACHED();
  return std::vector<device::BluetoothRemoteGattService*>();
}

device::BluetoothRemoteGattCharacteristic*
FakeRemoteGattService::GetCharacteristic(const std::string& identifier) const {
  NOTREACHED();
  return nullptr;
}

}  // namespace bluetooth
