// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_BLUETOOTH_INPUT_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_BLUETOOTH_INPUT_CLIENT_H_

#include <string>

#include "chromeos/dbus/bluetooth_input_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockBluetoothInputClient : public BluetoothInputClient {
 public:
  struct Properties : public BluetoothInputClient::Properties {
    Properties();
    virtual ~Properties();

    MOCK_METHOD0(ConnectSignals, void());

    MOCK_METHOD2(Get, void(dbus::PropertyBase* property,
                           dbus::PropertySet::GetCallback callback));
    MOCK_METHOD0(GetAll, void());
    MOCK_METHOD2(Set, void(dbus::PropertyBase* property,
                           dbus::PropertySet::SetCallback callback));

    MOCK_METHOD1(ChangedReceived, void(dbus::Signal*));
  };

  MockBluetoothInputClient();
  virtual ~MockBluetoothInputClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(GetProperties, Properties*(const dbus::ObjectPath&));
  MOCK_METHOD3(Connect, void(const dbus::ObjectPath&,
                             const ConnectCallback&,
                             const ConnectErrorCallback&));
  MOCK_METHOD2(Disconnect, void(const dbus::ObjectPath&,
                                const InputCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_BLUETOOTH_INPUT_CLIENT_H_
