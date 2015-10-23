// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"

#include "base/logging.h"

namespace device {

// static
scoped_ptr<BluetoothRemoteGattCharacteristicAndroid>
BluetoothRemoteGattCharacteristicAndroid::Create() {
  return make_scoped_ptr<BluetoothRemoteGattCharacteristicAndroid>(
      new BluetoothRemoteGattCharacteristicAndroid());
}

BluetoothRemoteGattCharacteristicAndroid::
    ~BluetoothRemoteGattCharacteristicAndroid() {}

std::string BluetoothRemoteGattCharacteristicAndroid::GetIdentifier() const {
  NOTIMPLEMENTED();
  return "";
}

BluetoothUUID BluetoothRemoteGattCharacteristicAndroid::GetUUID() const {
  NOTIMPLEMENTED();
  return BluetoothUUID();
}

bool BluetoothRemoteGattCharacteristicAndroid::IsLocal() const {
  return false;
}

const std::vector<uint8>& BluetoothRemoteGattCharacteristicAndroid::GetValue()
    const {
  NOTIMPLEMENTED();
  return value_;
}

BluetoothGattService* BluetoothRemoteGattCharacteristicAndroid::GetService()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicAndroid::GetProperties() const {
  NOTIMPLEMENTED();
  return 0;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicAndroid::GetPermissions() const {
  NOTIMPLEMENTED();
  return 0;
}

bool BluetoothRemoteGattCharacteristicAndroid::IsNotifying() const {
  NOTIMPLEMENTED();
  return false;
}

std::vector<BluetoothGattDescriptor*>
BluetoothRemoteGattCharacteristicAndroid::GetDescriptors() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothGattDescriptor*>();
}

BluetoothGattDescriptor*
BluetoothRemoteGattCharacteristicAndroid::GetDescriptor(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BluetoothRemoteGattCharacteristicAndroid::AddDescriptor(
    BluetoothGattDescriptor* descriptor) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothRemoteGattCharacteristicAndroid::UpdateValue(
    const std::vector<uint8>& value) {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothRemoteGattCharacteristicAndroid::StartNotifySession(
    const NotifySessionCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattCharacteristicAndroid::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattCharacteristicAndroid::WriteRemoteCharacteristic(
    const std::vector<uint8>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothRemoteGattCharacteristicAndroid::
    BluetoothRemoteGattCharacteristicAndroid() {}

}  // namespace device
