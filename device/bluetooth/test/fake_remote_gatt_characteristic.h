// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_CHARACTERISTIC_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_CHARACTERISTIC_H_

#include <string>
#include <vector>

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/public/interfaces/test/fake_bluetooth.mojom.h"

namespace device {
class BluetoothRemoteGattService;
class BluetoothRemoteGattDescriptor;
}  // namespace device

namespace bluetooth {

// Implements device::BluetoothRemoteGattCharacteristics. Meant to be used
// by FakeRemoteGattService to keep track of the characteristic's state and
// attributes.
class FakeRemoteGattCharacteristic
    : public device::BluetoothRemoteGattCharacteristic {
 public:
  FakeRemoteGattCharacteristic(const std::string& characteristic_id,
                               const device::BluetoothUUID& characteristic_uuid,
                               mojom::CharacteristicPropertiesPtr properties,
                               device::BluetoothRemoteGattService* service);
  ~FakeRemoteGattCharacteristic() override;

  // device::BluetoothGattCharacteristic overrides:
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // device::BluetoothRemoteGattCharacteristic overrides:
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothRemoteGattService* GetService() const override;
  std::vector<device::BluetoothRemoteGattDescriptor*> GetDescriptors()
      const override;
  device::BluetoothRemoteGattDescriptor* GetDescriptor(
      const std::string& identifier) const override;
  void ReadRemoteCharacteristic(const ValueCallback& callback,
                                const ErrorCallback& error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;

 protected:
  // device::BluetoothRemoteGattCharacteristic overrides:
  void SubscribeToNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;
  void UnsubscribeFromNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;

 private:
  const std::string characteristic_id_;
  const device::BluetoothUUID characteristic_uuid_;
  Properties properties_;
  device::BluetoothRemoteGattService* service_;
  std::vector<uint8_t> value_;
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_CHARACTERISTIC_H_
