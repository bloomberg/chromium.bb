// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_BLUETOOTH_NODE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_BLUETOOTH_NODE_CLIENT_H_

#include <string>

#include "chromeos/dbus/bluetooth_node_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockBluetoothNodeClient : public BluetoothNodeClient {
 public:
  MockBluetoothNodeClient();
  virtual ~MockBluetoothNodeClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(GetProperties, Properties*(const dbus::ObjectPath&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_BLUETOOTH_NODE_CLIENT_H_
