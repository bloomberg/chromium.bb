// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_NOTIFY_SESSION_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_NOTIFY_SESSION_H_

#include <string>

#include "base/callback.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothGattNotifySession : public BluetoothGattNotifySession {
 public:
  explicit MockBluetoothGattNotifySession(
      const std::string& characteristic_identifier);
  virtual ~MockBluetoothGattNotifySession();

  MOCK_CONST_METHOD0(GetCharacteristicIdentifier, std::string());
  MOCK_METHOD0(IsActive, bool());
  MOCK_METHOD1(Stop, void(const base::Closure&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothGattNotifySession);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_NOTIFY_SESSION_H_
