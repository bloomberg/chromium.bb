// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider.h"

namespace bluez {

// FakeBluetoothGattCharacteristicServiceProvider simulates behavior of a local
// GATT characteristic object and is used both in test cases in place of a mock
// and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothGattCharacteristicServiceProvider
    : public BluetoothGattCharacteristicServiceProvider {
 public:
  FakeBluetoothGattCharacteristicServiceProvider(
      const dbus::ObjectPath& object_path,
      std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate,
      const std::string& uuid,
      const std::vector<std::string>& flags,
      const std::vector<std::string>& permissions,
      const dbus::ObjectPath& service_path);
  ~FakeBluetoothGattCharacteristicServiceProvider() override;

  // BluetoothGattCharacteristicServiceProvider override.
  void SendValueChanged(const std::vector<uint8_t>& value) override;

  // Methods to simulate value get/set requests issued from a remote device. The
  // methods do nothing, if the associated service was not registered with the
  // GATT manager.
  void GetValue(
      const device::BluetoothLocalGattService::Delegate::ValueCallback&
          callback,
      const device::BluetoothLocalGattService::Delegate::ErrorCallback&
          error_callback);
  void SetValue(
      const std::vector<uint8_t>& value,
      const base::Closure& callback,
      const device::BluetoothLocalGattService::Delegate::ErrorCallback&
          error_callback);

  const dbus::ObjectPath& object_path() const override;
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
  std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattCharacteristicServiceProvider);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_
