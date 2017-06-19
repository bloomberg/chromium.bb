// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_SERVICE_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_SERVICE_H_

#include <string>
#include <vector>

#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {
class BluetoothDevice;
class BluetoothRemoteGattService;
class BluetoothRemoteGattCharacteristic;
}  // namespace device

namespace bluetooth {

// Implements device::BluetoothRemoteGattService. Meant to be used by
// FakePeripheral to keep track of the service's state and attributes.
class FakeRemoteGattService : public device::BluetoothRemoteGattService {
 public:
  FakeRemoteGattService(const std::string& service_id,
                        const device::BluetoothUUID& service_uuid,
                        bool is_primary,
                        device::BluetoothDevice* device);
  ~FakeRemoteGattService() override;

  // device::BluetoothGattService overrides:
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;

  // device::BluetoothRemoteGattService overrides:
  device::BluetoothDevice* GetDevice() const override;
  std::vector<device::BluetoothRemoteGattCharacteristic*> GetCharacteristics()
      const override;
  std::vector<device::BluetoothRemoteGattService*> GetIncludedServices()
      const override;
  device::BluetoothRemoteGattCharacteristic* GetCharacteristic(
      const std::string& identifier) const override;

 private:
  const std::string service_id_;
  const device::BluetoothUUID service_uuid_;
  const bool is_primary_;
  device::BluetoothDevice* device_;
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_SERVICE_H_
