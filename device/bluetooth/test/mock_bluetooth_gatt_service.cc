// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"

#include "device/bluetooth/test/mock_bluetooth_device.h"

using testing::Return;
using testing::_;

namespace device {

MockBluetoothGattService::MockBluetoothGattService(
    MockBluetoothDevice* device,
    const std::string& identifier,
    const BluetoothUUID& uuid,
    bool is_primary,
    bool is_local) {
  ON_CALL(*this, GetIdentifier()).WillByDefault(Return(identifier));
  ON_CALL(*this, GetUUID()).WillByDefault(Return(uuid));
  ON_CALL(*this, IsLocal()).WillByDefault(Return(is_local));
  ON_CALL(*this, IsPrimary()).WillByDefault(Return(is_primary));
  ON_CALL(*this, GetDevice()).WillByDefault(Return(device));
  ON_CALL(*this, GetCharacteristics())
      .WillByDefault(Return(std::vector<BluetoothGattCharacteristic*>()));
  ON_CALL(*this, GetIncludedServices())
      .WillByDefault(Return(std::vector<BluetoothGattService*>()));
  ON_CALL(*this, GetCharacteristic(_))
      .WillByDefault(Return(static_cast<BluetoothGattCharacteristic*>(NULL)));
}

MockBluetoothGattService::~MockBluetoothGattService() {
}

}  // namespace device
