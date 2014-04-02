// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_

#include <string>
#include <vector>

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_gatt_characteristic_service_provider.h"
#include "dbus/object_path.h"

namespace chromeos {

// FakeBluetoothGattCharacteristicServiceProvider simulates behavior of a local
// GATT characteristic object and is used both in test cases in place of a mock
// and on the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothGattCharacteristicServiceProvider
    : public BluetoothGattCharacteristicServiceProvider {
 public:
  FakeBluetoothGattCharacteristicServiceProvider(
      const dbus::ObjectPath& object_path,
      Delegate* delegate,
      const std::string& uuid,
      const std::vector<std::string>& flags,
      const std::vector<std::string>& permissions,
      const dbus::ObjectPath& service_path);
  virtual ~FakeBluetoothGattCharacteristicServiceProvider();

  // BluetoothGattCharacteristicServiceProvider override.
  virtual void SendValueChanged(const std::vector<uint8>& value) OVERRIDE;

  // Methods to simulate value get/set requests issued from a remote device. The
  // methods do nothing, if the associated service was not registered with the
  // GATT manager.
  void GetValue(const Delegate::ValueCallback& callback,
                const Delegate::ErrorCallback& error_callback);
  void SetValue(const std::vector<uint8>& value,
                const base::Closure& callback,
                const Delegate::ErrorCallback& error_callback);

  const dbus::ObjectPath& object_path() const { return object_path_; }
  const std::string& uuid() const { return uuid_; }
  const dbus::ObjectPath& service_path() const { return service_path_; }

 private:
  // D-Bus object path of the fake GATT characteristic.
  dbus::ObjectPath object_path_;

  // 128-bit GATT characteristic UUID.
  std::string uuid_;

  // Object path of the service that this characteristic belongs to.
  dbus::ObjectPath service_path_;

  // The delegate that method calls are passed on to.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattCharacteristicServiceProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_
