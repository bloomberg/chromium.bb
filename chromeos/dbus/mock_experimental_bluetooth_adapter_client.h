// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_EXPERIMENTAL_BLUETOOTH_ADAPTER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_EXPERIMENTAL_BLUETOOTH_ADAPTER_CLIENT_H_

#include <string>

#include "chromeos/dbus/experimental_bluetooth_adapter_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockExperimentalBluetoothAdapterClient
    : public ExperimentalBluetoothAdapterClient {
 public:
  struct Properties : public ExperimentalBluetoothAdapterClient::Properties {
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

  MockExperimentalBluetoothAdapterClient();
  virtual ~MockExperimentalBluetoothAdapterClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD0(GetAdapters, std::vector<dbus::ObjectPath>());
  MOCK_METHOD1(GetProperties, Properties*(const dbus::ObjectPath&));
  MOCK_METHOD3(StartDiscovery, void(const dbus::ObjectPath&,
                                    const base::Closure&,
                                    const ErrorCallback&));
  MOCK_METHOD3(StopDiscovery, void(const dbus::ObjectPath&,
                                   const base::Closure&,
                                   const ErrorCallback&));
  MOCK_METHOD4(RemoveDevice, void(const dbus::ObjectPath&,
                                  const dbus::ObjectPath&,
                                  const base::Closure&,
                                  const ErrorCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_EXPERIMENTAL_BLUETOOTH_ADAPTER_CLIENT_H_
