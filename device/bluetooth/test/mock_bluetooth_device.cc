// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_device.h"

#include "base/utf_string_conversions.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"

namespace device {

MockBluetoothDevice::MockBluetoothDevice(MockBluetoothAdapter* adapter,
                                         const std::string& name,
                                         const std::string& address,
                                         bool paired,
                                         bool bonded,
                                         bool connected)
    : name_(UTF8ToUTF16(name)),
      address_(address) {
  ON_CALL(*this, GetName())
      .WillByDefault(testing::Return(name_));
  ON_CALL(*this, address())
      .WillByDefault(testing::ReturnRef(address_));
  ON_CALL(*this, IsPaired())
      .WillByDefault(testing::Return(paired));
  ON_CALL(*this, IsBonded())
      .WillByDefault(testing::Return(bonded));
  ON_CALL(*this, IsConnected())
      .WillByDefault(testing::Return(connected));
  ON_CALL(*this, ExpectingPinCode())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, ExpectingPasskey())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, ExpectingConfirmation())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, GetServices())
      .WillByDefault(testing::ReturnRef(service_list_));
}

MockBluetoothDevice::~MockBluetoothDevice() {}

}  // namespace device
