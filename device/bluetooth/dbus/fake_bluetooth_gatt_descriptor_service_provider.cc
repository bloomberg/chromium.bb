// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_service_provider.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"

namespace bluez {

FakeBluetoothGattDescriptorServiceProvider::
    FakeBluetoothGattDescriptorServiceProvider(
        const dbus::ObjectPath& object_path,
        Delegate* delegate,
        const std::string& uuid,
        const std::vector<std::string>& permissions,
        const dbus::ObjectPath& characteristic_path)
    : object_path_(object_path),
      uuid_(uuid),
      characteristic_path_(characteristic_path),
      delegate_(delegate) {
  VLOG(1) << "Creating Bluetooth GATT descriptor: " << object_path_.value();

  DCHECK(object_path_.IsValid());
  DCHECK(characteristic_path_.IsValid());
  DCHECK(!uuid.empty());
  DCHECK(delegate_);
  DCHECK(base::StartsWith(object_path_.value(),
                          characteristic_path_.value() + "/",
                          base::CompareCase::SENSITIVE));

  // TODO(armansito): Do something with |permissions|.

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->RegisterDescriptorServiceProvider(this);
}

FakeBluetoothGattDescriptorServiceProvider::
    ~FakeBluetoothGattDescriptorServiceProvider() {
  VLOG(1) << "Cleaning up Bluetooth GATT descriptor: " << object_path_.value();

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->UnregisterDescriptorServiceProvider(this);
}

void FakeBluetoothGattDescriptorServiceProvider::SendValueChanged(
    const std::vector<uint8_t>& value) {
  VLOG(1) << "Sent descriptor value changed: " << object_path_.value()
          << " UUID: " << uuid_;
}

void FakeBluetoothGattDescriptorServiceProvider::GetValue(
    const Delegate::ValueCallback& callback,
    const Delegate::ErrorCallback& error_callback) {
  VLOG(1) << "GATT descriptor value Get request: " << object_path_.value()
          << " UUID: " << uuid_;

  // Check if this descriptor is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  FakeBluetoothGattCharacteristicServiceProvider* characteristic =
      fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
          characteristic_path_);
  if (!characteristic) {
    VLOG(1) << "GATT characteristic for descriptor does not exist: "
            << characteristic_path_.value();
    return;
  }
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(
          characteristic->service_path())) {
    VLOG(1) << "GATT descriptor not registered.";
    error_callback.Run();
    return;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  delegate_->GetDescriptorValue(callback, error_callback);
}

void FakeBluetoothGattDescriptorServiceProvider::SetValue(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const Delegate::ErrorCallback& error_callback) {
  VLOG(1) << "GATT descriptor value Set request: " << object_path_.value()
          << " UUID: " << uuid_;

  // Check if this descriptor is registered.
  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  FakeBluetoothGattCharacteristicServiceProvider* characteristic =
      fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
          characteristic_path_);
  if (!characteristic) {
    VLOG(1) << "GATT characteristic for descriptor does not exist: "
            << characteristic_path_.value();
    return;
  }
  if (!fake_bluetooth_gatt_manager_client->IsServiceRegistered(
          characteristic->service_path())) {
    VLOG(1) << "GATT descriptor not registered.";
    error_callback.Run();
    return;
  }

  // Pass on to the delegate.
  DCHECK(delegate_);
  delegate_->SetDescriptorValue(value, callback, error_callback);
}

}  // namespace bluez
