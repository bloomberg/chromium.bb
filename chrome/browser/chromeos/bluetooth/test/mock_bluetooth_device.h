// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_

#include <string>

#include "base/string16.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockBluetoothDevice : public BluetoothDevice {
 public:
  MockBluetoothDevice(MockBluetoothAdapter* adapter,
                      const std::string& name,
                      const std::string& address,
                      bool paired,
                      bool bonded,
                      bool connected);
  virtual ~MockBluetoothDevice();

  MOCK_CONST_METHOD0(address, const std::string&());
  MOCK_CONST_METHOD0(GetName, string16());
  MOCK_CONST_METHOD0(IsPaired, bool());
  MOCK_CONST_METHOD0(IsBonded, bool());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD1(ProvidesServiceWithUUID, bool(const std::string&));

  MOCK_METHOD2(ProvidesServiceWithName,
      void(const std::string&,
           const ProvidesServiceCallback& callback));
  MOCK_METHOD3(SetOutOfBandPairingData,
      void(const chromeos::BluetoothOutOfBandPairingData& data,
           const base::Closure& callback,
           const BluetoothDevice::ErrorCallback& error_callback));
  MOCK_METHOD2(ClearOutOfBandPairingData,
      void(const base::Closure& callback,
           const BluetoothDevice::ErrorCallback& error_callback));

 private:
  string16 name_;
  std::string address_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_TEST_MOCK_BLUETOOTH_DEVICE_H_
