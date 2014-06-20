// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CONNECTION_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CONNECTION_H_

#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothGattConnection : public BluetoothGattConnection {
 public:
  MockBluetoothGattConnection(const std::string& device_address);
  virtual ~MockBluetoothGattConnection();

  MOCK_CONST_METHOD0(GetDeviceAddress, std::string());
  MOCK_METHOD0(IsConnected, bool());
  MOCK_METHOD1(Disconnect, void(const base::Closure&));
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CONNECTION_H_
