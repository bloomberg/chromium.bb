// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"

namespace bluez {

FakeBluetoothGattCharacteristicServiceProvider::
    FakeBluetoothGattCharacteristicServiceProvider(
        const dbus::ObjectPath& object_path,
        Delegate* delegate,
        const std::string& uuid,
        const std::vector<std::string>& flags,
        const std::vector<std::string>& permissions,
        const dbus::ObjectPath& service_path)
    : object_path_(object_path),
      uuid_(uuid),
      service_path_(service_path),
      delegate_(delegate) {
  VLOG(1) << "Creating Bluetooth GATT characteristic: " << object_path_.value();

  DCHECK(object_path_.IsValid());
  DCHECK(service_path_.IsValid());
  DCHECK(!uuid.empty());
  DCHECK(delegate_);
  DCHECK(base::StartsWith(object_path_.value(), service_path_.value() + "/",
                          base::CompareCase::SENSITIVE));

  // TODO(armansito): Do something with |flags| and |permissions|.

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->RegisterCharacteristicServiceProvider(
      this);
}

FakeBluetoothGattCharacteristicServiceProvider::
    ~FakeBluetoothGattCharacteristicServiceProvider() {
  VLOG(1) << "Cleaning up Bluetooth GATT characteristic: "
          << object_path_.value();

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->UnregisterCharacteristicServiceProvider(
      this);
}

void FakeBluetoothGattCharacteristicServiceProvider::SendValueChanged(
    const std::vector<uint8_t>& value) {
  VLOG(1) << "Sent characteristic value changed: " << object_path_.value()
          << " UUID: " << uuid_;
}

void FakeBluetoothGattCharacteristicServiceProvider::GetValue(
    const Delegate::ValueCallback& callback,
    const Delegate::ErrorCallback& error_callback) {
  VLOG(1) << "GATT characteristic value Get request: " << object_path_.value()
          << " UUID: " << uuid_;

  // Check if this characteristic is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(service_path_)) {
    VLOG(1) << "GATT characteristic not registered.";
    error_callback.Run();
    return;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  delegate_->GetCharacteristicValue(callback, error_callback);
}

void FakeBluetoothGattCharacteristicServiceProvider::SetValue(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const Delegate::ErrorCallback& error_callback) {
  VLOG(1) << "GATT characteristic value Set request: " << object_path_.value()
          << " UUID: " << uuid_;

  // Check if this characteristic is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(service_path_)) {
    VLOG(1) << "GATT characteristic not registered.";
    error_callback.Run();
    return;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  delegate_->SetCharacteristicValue(value, callback, error_callback);
}

}  // namespace bluez
