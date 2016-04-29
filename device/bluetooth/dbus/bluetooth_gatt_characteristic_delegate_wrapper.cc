// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_delegate_wrapper.h"

#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"

namespace bluez {

BluetoothGattCharacteristicDelegateWrapper::
    BluetoothGattCharacteristicDelegateWrapper(
        BluetoothLocalGattServiceBlueZ* service,
        BluetoothLocalGattCharacteristicBlueZ* characteristic)
    : service_(service), characteristic_(characteristic) {}

void BluetoothGattCharacteristicDelegateWrapper::GetValue(
    const device::BluetoothLocalGattService::Delegate::ValueCallback& callback,
    const device::BluetoothLocalGattService::Delegate::ErrorCallback&
        error_callback) {
  service_->GetDelegate()->OnCharacteristicReadRequest(
      service_, characteristic_, 0, callback, error_callback);
}

void BluetoothGattCharacteristicDelegateWrapper::SetValue(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const device::BluetoothLocalGattService::Delegate::ErrorCallback&
        error_callback) {
  service_->GetDelegate()->OnCharacteristicWriteRequest(
      service_, characteristic_, value, 0, callback, error_callback);
}

}  // namespace bluez
