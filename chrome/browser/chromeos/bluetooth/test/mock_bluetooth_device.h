// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockBluetoothDevice : public BluetoothDevice {
 public:
  explicit MockBluetoothDevice(MockBluetoothAdapter* adapter,
      const std::string& name, const std::string& address);
  virtual ~MockBluetoothDevice();

  MOCK_CONST_METHOD0(address, const std::string&());
  MOCK_CONST_METHOD0(GetName, string16());
  MOCK_CONST_METHOD1(ProvidesServiceWithUUID, bool(const std::string&));

 private:
  string16 name_;
  std::string address_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
