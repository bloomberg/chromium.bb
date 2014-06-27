// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"

using testing::Return;

namespace device {

MockBluetoothGattNotifySession::MockBluetoothGattNotifySession(
    const std::string& characteristic_identifier) {
  ON_CALL(*this, GetCharacteristicIdentifier())
      .WillByDefault(Return(characteristic_identifier));
  ON_CALL(*this, IsActive()).WillByDefault(Return(true));
}

MockBluetoothGattNotifySession::~MockBluetoothGattNotifySession() {
}

}  // namespace device
