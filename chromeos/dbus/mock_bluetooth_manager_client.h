// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_BLUETOOTH_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_BLUETOOTH_MANAGER_CLIENT_H_

#include <string>

#include "chromeos/dbus/bluetooth_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockBluetoothManagerClient : public BluetoothManagerClient {
 public:
  MockBluetoothManagerClient();
  virtual ~MockBluetoothManagerClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD0(GetProperties, Properties*());
  MOCK_METHOD1(DefaultAdapter, void(const AdapterCallback& callback));
  MOCK_METHOD2(FindAdapter, void(const std::string&,
                                 const AdapterCallback& callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_BLUETOOTH_MANAGER_CLIENT_H_
