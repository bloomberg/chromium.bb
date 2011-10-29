// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MOCK_BLUETOOTH_MANAGER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MOCK_BLUETOOTH_MANAGER_CLIENT_H_

#include <string>

#include "chrome/browser/chromeos/dbus/bluetooth_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockBluetoothManagerClient : public BluetoothManagerClient {
 public:
  MockBluetoothManagerClient();
  virtual ~MockBluetoothManagerClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(DefaultAdapter,
               void(const DefaultAdapterCallback& callback));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MOCK_BLUETOOTH_MANAGER_CLIENT_H_
