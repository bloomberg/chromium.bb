// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"

using ::testing::Return;

namespace device {

MockBluetoothGattConnection::MockBluetoothGattConnection(
    const std::string& device_address) {
  ON_CALL(*this, GetDeviceAddress()).WillByDefault(Return(device_address));
  ON_CALL(*this, IsConnected()).WillByDefault(Return(true));
}

MockBluetoothGattConnection::~MockBluetoothGattConnection() {}

}  // namespace device
