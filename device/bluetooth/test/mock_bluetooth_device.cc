// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_device.h"

#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"

namespace device {

MockBluetoothDevice::MockBluetoothDevice(MockBluetoothAdapter* adapter,
                                         uint32 bluetooth_class,
                                         const std::string& name,
                                         const std::string& address,
                                         bool paired,
                                         bool connected)
    : bluetooth_class_(bluetooth_class),
      name_(name),
      address_(address) {
  ON_CALL(*this, GetBluetoothClass())
      .WillByDefault(testing::Return(bluetooth_class_));
  ON_CALL(*this, GetDeviceName())
      .WillByDefault(testing::Return(name_));
  ON_CALL(*this, GetAddress())
      .WillByDefault(testing::Return(address_));
  ON_CALL(*this, GetDeviceType())
      .WillByDefault(testing::Return(DEVICE_UNKNOWN));
  ON_CALL(*this, GetRSSI())
      .WillByDefault(testing::Return(kUnknownPower));
  ON_CALL(*this, GetCurrentHostTransmitPower())
      .WillByDefault(testing::Return(kUnknownPower));
  ON_CALL(*this, GetMaximumHostTransmitPower())
      .WillByDefault(testing::Return(kUnknownPower));
  ON_CALL(*this, GetVendorIDSource())
      .WillByDefault(testing::Return(VENDOR_ID_UNKNOWN));
  ON_CALL(*this, GetVendorID())
      .WillByDefault(testing::Return(0));
  ON_CALL(*this, GetProductID())
      .WillByDefault(testing::Return(0));
  ON_CALL(*this, GetDeviceID())
      .WillByDefault(testing::Return(0));
  ON_CALL(*this, IsPaired())
      .WillByDefault(testing::Return(paired));
  ON_CALL(*this, IsConnected())
      .WillByDefault(testing::Return(connected));
  ON_CALL(*this, IsConnectable())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, IsConnecting())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, GetName())
      .WillByDefault(testing::Return(base::UTF8ToUTF16(name_)));
  ON_CALL(*this, ExpectingPinCode())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, ExpectingPasskey())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, ExpectingConfirmation())
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, GetUUIDs())
      .WillByDefault(testing::Return(uuids_));
}

MockBluetoothDevice::~MockBluetoothDevice() {}

}  // namespace device
