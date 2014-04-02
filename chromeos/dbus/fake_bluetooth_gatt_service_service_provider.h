// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_SERVICE_SERVICE_PROVIDER_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_SERVICE_SERVICE_PROVIDER_H_

#include <string>
#include <vector>

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_gatt_service_service_provider.h"
#include "dbus/object_path.h"

namespace chromeos {

// FakeBluetoothGattServiceServiceProvider simulates behavior of a local GATT
// service object and is used both in test cases in place of a mock and on the
// Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothGattServiceServiceProvider
    : public BluetoothGattServiceServiceProvider {
 public:
  FakeBluetoothGattServiceServiceProvider(
      const dbus::ObjectPath& object_path,
      const std::string& uuid,
      const std::vector<dbus::ObjectPath>& includes);
  virtual ~FakeBluetoothGattServiceServiceProvider();

  const dbus::ObjectPath& object_path() const { return object_path_; }
  const std::string& uuid() const { return uuid_; }

 private:
  // D-Bus object path of the fake GATT service.
  dbus::ObjectPath object_path_;

  // 128-bit GATT service UUID.
  std::string uuid_;

  // List of included GATT services.
  std::vector<dbus::ObjectPath> includes_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattServiceServiceProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_SERVICE_SERVICE_PROVIDER_H_
