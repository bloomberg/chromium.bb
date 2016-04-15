// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_characteristic_bluez.h"

#include <iostream>
#include <string>

#include "base/callback_forward.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_local_gatt_service_bluez.h"

namespace bluez {

BluetoothLocalGattCharacteristicBlueZ::BluetoothLocalGattCharacteristicBlueZ(
    BluetoothLocalGattServiceBlueZ* service,
    const dbus::ObjectPath& object_path)
    : BluetoothGattCharacteristicBlueZ(service, object_path),
      weak_ptr_factory_(this) {
  VLOG(1) << "Creating local GATT characteristic with identifier: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();
}

BluetoothLocalGattCharacteristicBlueZ::
    ~BluetoothLocalGattCharacteristicBlueZ() {}

device::BluetoothUUID BluetoothLocalGattCharacteristicBlueZ::GetUUID() const {
  NOTIMPLEMENTED();
  return device::BluetoothUUID();
}

bool BluetoothLocalGattCharacteristicBlueZ::IsLocal() const {
  return true;
}

const std::vector<uint8_t>& BluetoothLocalGattCharacteristicBlueZ::GetValue()
    const {
  NOTIMPLEMENTED();
  static std::vector<uint8_t>* temp = new std::vector<uint8_t>;
  return *temp;
}

bool BluetoothLocalGattCharacteristicBlueZ::AddDescriptor(
    device::BluetoothGattDescriptor* descriptor) {
  NOTIMPLEMENTED();
  return false;
}

device::BluetoothGattCharacteristic::Properties
BluetoothLocalGattCharacteristicBlueZ::GetProperties() const {
  NOTIMPLEMENTED();
  return PROPERTY_NONE;
}

bool BluetoothLocalGattCharacteristicBlueZ::IsNotifying() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothLocalGattCharacteristicBlueZ::UpdateValue(
    const std::vector<uint8_t>& value) {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothLocalGattCharacteristicBlueZ::StartNotifySession(
    const NotifySessionCallback& callback,
    const ErrorCallback& error_callback) {
  // Doesn't apply for a local characteristic.
  NOTIMPLEMENTED();
}

void BluetoothLocalGattCharacteristicBlueZ::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  // Doesn't apply for a local characteristic.
  NOTIMPLEMENTED();
}

void BluetoothLocalGattCharacteristicBlueZ::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // Doesn't apply for a local characteristic.
  NOTIMPLEMENTED();
}

}  // namespace bluez
