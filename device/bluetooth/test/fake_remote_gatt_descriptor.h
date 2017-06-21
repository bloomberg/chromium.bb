// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_DESCRIPTOR_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_DESCRIPTOR_H_

#include <string>
#include <vector>

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace bluetooth {

// Implements device::BluetoothRemoteGattDescriptors. Meant to be used
// by FakeRemoteGattCharacteristic to keep track of the descriptor's state and
// attributes.
class FakeRemoteGattDescriptor : public device::BluetoothRemoteGattDescriptor {
 public:
  FakeRemoteGattDescriptor(
      const std::string& descriptor_id,
      const device::BluetoothUUID& descriptor_uuid,
      device::BluetoothRemoteGattCharacteristic* characteristic);
  ~FakeRemoteGattDescriptor() override;

  // device::BluetoothGattDescriptor overrides:
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  device::BluetoothRemoteGattCharacteristic::Permissions GetPermissions()
      const override;

  // device::BluetoothRemoteGattDescriptor overrides:
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  void ReadRemoteDescriptor(const ValueCallback& callback,
                            const ErrorCallback& error_callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& value,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) override;

 private:
  const std::string descriptor_id_;
  const device::BluetoothUUID descriptor_uuid_;
  device::BluetoothRemoteGattCharacteristic* characteristic_;
  std::vector<uint8_t> value_;
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_DESCRIPTOR_H_
