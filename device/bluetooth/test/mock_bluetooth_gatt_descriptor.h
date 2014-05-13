// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_DESCRIPTOR_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_DESCRIPTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothGattCharacteristic;

class MockBluetoothGattDescriptor : public BluetoothGattDescriptor {
 public:
  MockBluetoothGattDescriptor(
      MockBluetoothGattCharacteristic* characteristic,
      const std::string& identifier,
      const BluetoothUUID& uuid,
      bool is_local,
      BluetoothGattCharacteristic::Permissions permissions);
  virtual ~MockBluetoothGattDescriptor();

  MOCK_CONST_METHOD0(GetIdentifier, std::string());
  MOCK_CONST_METHOD0(GetUUID, BluetoothUUID());
  MOCK_CONST_METHOD0(IsLocal, bool());
  MOCK_CONST_METHOD0(GetValue, const std::vector<uint8>&());
  MOCK_CONST_METHOD0(GetCharacteristic, BluetoothGattCharacteristic*());
  MOCK_CONST_METHOD0(GetPermissions,
                     BluetoothGattCharacteristic::Permissions());
  MOCK_METHOD2(ReadRemoteDescriptor,
               void(const ValueCallback&, const ErrorCallback&));
  MOCK_METHOD3(WriteRemoteDescriptor,
               void(const std::vector<uint8>&,
                    const base::Closure&,
                    const ErrorCallback&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothGattDescriptor);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_DESCRIPTOR_H_
