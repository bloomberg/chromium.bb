// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MOCK_BLUETOOTH_ADAPTER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MOCK_BLUETOOTH_ADAPTER_CLIENT_H_

#include <string>

#include "chrome/browser/chromeos/dbus/bluetooth_adapter_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockBluetoothAdapterClient : public BluetoothAdapterClient {
 public:
  MockBluetoothAdapterClient();
  virtual ~MockBluetoothAdapterClient();

  MOCK_METHOD2(AddObserver, void(Observer*, const std::string&));
  MOCK_METHOD2(RemoveObserver, void(Observer*, const std::string&));
  MOCK_METHOD1(StartDiscovery, void(const std::string&));
  MOCK_METHOD1(StopDiscovery, void(const std::string&));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MOCK_BLUETOOTH_ADAPTER_CLIENT_H_
