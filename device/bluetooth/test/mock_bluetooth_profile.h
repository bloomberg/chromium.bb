// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_PROFILE_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_PROFILE_H_

#include "device/bluetooth/bluetooth_profile.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothProfile : public BluetoothProfile {
 public:
  MockBluetoothProfile();
  virtual ~MockBluetoothProfile();

  MOCK_METHOD0(Unregister, void());
  MOCK_METHOD1(SetConnectionCallback,
               void(const BluetoothProfile::ConnectionCallback&));
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_PROFILE_H_
