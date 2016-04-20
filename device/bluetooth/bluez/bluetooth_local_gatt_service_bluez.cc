// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace bluez {

BluetoothLocalGattServiceBlueZ::BluetoothLocalGattServiceBlueZ(
    BluetoothAdapterBlueZ* adapter,
    const dbus::ObjectPath& object_path)
    : BluetoothGattServiceBlueZ(adapter, object_path), weak_ptr_factory_(this) {
  VLOG(1) << "Creating local GATT service with identifier: "
          << object_path.value();
}

BluetoothLocalGattServiceBlueZ::~BluetoothLocalGattServiceBlueZ() {}

void BluetoothLocalGattServiceBlueZ::Register(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(bluez::BluezDBusManager::Get());
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattManagerClient()
      ->RegisterService(
          object_path(), BluetoothGattManagerClient::Options(), callback,
          base::Bind(&BluetoothLocalGattServiceBlueZ::OnRegistrationError,
                     weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothLocalGattServiceBlueZ::Unregister(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(bluez::BluezDBusManager::Get());
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattManagerClient()
      ->UnregisterService(
          object_path(), callback,
          base::Bind(&BluetoothLocalGattServiceBlueZ::OnRegistrationError,
                     weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothLocalGattServiceBlueZ::OnRegistrationError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "[Un]Register Service failed: " << error_name
          << ", message: " << error_message;
  error_callback.Run(
      BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name));
}

}  // namespace bluez
