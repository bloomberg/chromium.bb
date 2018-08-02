// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_winrt.h"

#include "base/logging.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

BluetoothRemoteGattDescriptorWinrt::BluetoothRemoteGattDescriptorWinrt() =
    default;

BluetoothRemoteGattDescriptorWinrt::~BluetoothRemoteGattDescriptorWinrt() =
    default;

std::string BluetoothRemoteGattDescriptorWinrt::GetIdentifier() const {
  NOTIMPLEMENTED();
  return std::string();
}

BluetoothUUID BluetoothRemoteGattDescriptorWinrt::GetUUID() const {
  NOTIMPLEMENTED();
  return BluetoothUUID();
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorWinrt::GetPermissions() const {
  NOTIMPLEMENTED();
  return BluetoothGattCharacteristic::Permissions();
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorWinrt::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorWinrt::GetCharacteristic() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void BluetoothRemoteGattDescriptorWinrt::ReadRemoteDescriptor(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattDescriptorWinrt::WriteRemoteDescriptor(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

}  // namespace device
