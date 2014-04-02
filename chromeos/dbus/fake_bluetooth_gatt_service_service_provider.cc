// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_gatt_service_service_provider.h"

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_gatt_manager_client.h"

namespace chromeos {

FakeBluetoothGattServiceServiceProvider::
    FakeBluetoothGattServiceServiceProvider(
        const dbus::ObjectPath& object_path,
        const std::string& uuid,
        const std::vector<dbus::ObjectPath>& includes)
        : object_path_(object_path),
          uuid_(uuid),
          includes_(includes) {
  VLOG(1) << "Creating Bluetooth GATT service: " << object_path_.value();

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          DBusThreadManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->RegisterServiceServiceProvider(this);
}

FakeBluetoothGattServiceServiceProvider::
    ~FakeBluetoothGattServiceServiceProvider() {
  VLOG(1) << "Cleaning up Bluetooth GATT service: " << object_path_.value();

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          DBusThreadManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->UnregisterServiceServiceProvider(this);
}

}  // namespace chromeos
