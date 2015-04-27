// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_le_advertisement_service_provider.h"
#include "chromeos/dbus/fake_bluetooth_le_advertising_manager_client.h"

namespace chromeos {

FakeBluetoothLEAdvertisementServiceProvider::
    FakeBluetoothLEAdvertisementServiceProvider(
        const dbus::ObjectPath& object_path,
        Delegate* delegate)
    : delegate_(delegate) {
  object_path_ = object_path;
  VLOG(1) << "Creating Bluetooth Advertisement: " << object_path_.value();

  FakeBluetoothLEAdvertisingManagerClient*
      fake_bluetooth_profile_manager_client =
          static_cast<FakeBluetoothLEAdvertisingManagerClient*>(
              DBusThreadManager::Get()
                  ->GetBluetoothLEAdvertisingManagerClient());
  fake_bluetooth_profile_manager_client->RegisterAdvertisementServiceProvider(
      this);
}

FakeBluetoothLEAdvertisementServiceProvider::
    ~FakeBluetoothLEAdvertisementServiceProvider() {
  VLOG(1) << "Cleaning up Bluetooth Advertisement: " << object_path_.value();

  FakeBluetoothLEAdvertisingManagerClient*
      fake_bluetooth_profile_manager_client =
          static_cast<FakeBluetoothLEAdvertisingManagerClient*>(
              DBusThreadManager::Get()
                  ->GetBluetoothLEAdvertisingManagerClient());
  fake_bluetooth_profile_manager_client->UnregisterAdvertisementServiceProvider(
      this);
}

void FakeBluetoothLEAdvertisementServiceProvider::Release() {
  VLOG(1) << object_path_.value() << ": Release";
  delegate_->Released();
}

}  // namespace chromeos
