// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_SERVICE_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_SERVICE_H_

#include <string>
#include <vector>

#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class BluetoothGattCharacteristic;
class MockBluetoothDevice;

class MockBluetoothGattService : public BluetoothGattService {
 public:
  MockBluetoothGattService(MockBluetoothDevice* device,
                           const std::string& identifier,
                           const BluetoothUUID& uuid,
                           bool is_primary,
                           bool is_local);
  virtual ~MockBluetoothGattService();

  MOCK_CONST_METHOD0(GetIdentifier, std::string());
  MOCK_CONST_METHOD0(GetUUID, BluetoothUUID());
  MOCK_CONST_METHOD0(IsLocal, bool());
  MOCK_CONST_METHOD0(IsPrimary, bool());
  MOCK_CONST_METHOD0(GetDevice, BluetoothDevice*());
  MOCK_CONST_METHOD0(GetCharacteristics,
                     std::vector<BluetoothGattCharacteristic*>());
  MOCK_CONST_METHOD0(GetIncludedServices, std::vector<BluetoothGattService*>());
  MOCK_CONST_METHOD1(GetCharacteristic,
                     BluetoothGattCharacteristic*(const std::string&));
  MOCK_METHOD1(AddCharacteristic, bool(BluetoothGattCharacteristic*));
  MOCK_METHOD1(AddIncludedService, bool(BluetoothGattService*));
  MOCK_METHOD2(Register, void(const base::Closure&, const ErrorCallback&));
  MOCK_METHOD2(Unregister, void(const base::Closure&, const ErrorCallback&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothGattService);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_SERVICE_H_
