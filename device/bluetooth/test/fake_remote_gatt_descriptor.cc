// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_remote_gatt_descriptor.h"

namespace bluetooth {

FakeRemoteGattDescriptor::FakeRemoteGattDescriptor(
    const std::string& descriptor_id,
    const device::BluetoothUUID& descriptor_uuid,
    device::BluetoothRemoteGattCharacteristic* characteristic)
    : descriptor_id_(descriptor_id),
      descriptor_uuid_(descriptor_uuid),
      characteristic_(characteristic) {}

FakeRemoteGattDescriptor::~FakeRemoteGattDescriptor() {}

std::string FakeRemoteGattDescriptor::GetIdentifier() const {
  return descriptor_id_;
}

device::BluetoothUUID FakeRemoteGattDescriptor::GetUUID() const {
  return descriptor_uuid_;
}

device::BluetoothRemoteGattCharacteristic::Permissions
FakeRemoteGattDescriptor::GetPermissions() const {
  NOTREACHED();
  return device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE;
}

const std::vector<uint8_t>& FakeRemoteGattDescriptor::GetValue() const {
  NOTREACHED();
  return value_;
}

device::BluetoothRemoteGattCharacteristic*
FakeRemoteGattDescriptor::GetCharacteristic() const {
  return characteristic_;
}

void FakeRemoteGattDescriptor::ReadRemoteDescriptor(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {}

void FakeRemoteGattDescriptor::WriteRemoteDescriptor(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {}

}  // namespace bluetooth
