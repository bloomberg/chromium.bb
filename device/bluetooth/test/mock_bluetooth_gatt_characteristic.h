// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CHARACTERISTIC_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CHARACTERISTIC_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class BluetoothGattDescriptor;
class BluetoothGattService;
class MockBluetoothGattService;

class MockBluetoothGattCharacteristic : public BluetoothGattCharacteristic {
 public:
  MockBluetoothGattCharacteristic(MockBluetoothGattService* service,
                                  const std::string& identifier,
                                  const BluetoothUUID& uuid,
                                  bool is_local,
                                  Properties properties,
                                  Permissions permissions);
  virtual ~MockBluetoothGattCharacteristic();

  MOCK_CONST_METHOD0(GetIdentifier, std::string());
  MOCK_CONST_METHOD0(GetUUID, BluetoothUUID());
  MOCK_CONST_METHOD0(IsLocal, bool());
  MOCK_CONST_METHOD0(GetValue, const std::vector<uint8>&());
  MOCK_CONST_METHOD0(GetService, BluetoothGattService*());
  MOCK_CONST_METHOD0(GetProperties, Properties());
  MOCK_CONST_METHOD0(GetPermissions, Permissions());
  MOCK_CONST_METHOD0(IsNotifying, bool());
  MOCK_CONST_METHOD0(GetDescriptors, std::vector<BluetoothGattDescriptor*>());
  MOCK_CONST_METHOD1(GetDescriptor,
                     BluetoothGattDescriptor*(const std::string&));
  MOCK_METHOD1(AddDescriptor, bool(BluetoothGattDescriptor*));
  MOCK_METHOD1(UpdateValue, bool(const std::vector<uint8>&));
  MOCK_METHOD2(StartNotifySession,
               void(const NotifySessionCallback&, const ErrorCallback&));
  MOCK_METHOD2(ReadRemoteCharacteristic,
               void(const ValueCallback&, const ErrorCallback&));
  MOCK_METHOD3(WriteRemoteCharacteristic,
               void(const std::vector<uint8>&,
                    const base::Closure&,
                    const ErrorCallback&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothGattCharacteristic);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CHARACTERISTIC_H_
