// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_BLUETOOTH_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_BLUETOOTH_DEVICE_CLIENT_H_

#include <string>

#include "chromeos/dbus/bluetooth_device_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockBluetoothDeviceClient : public BluetoothDeviceClient {
 public:
  MockBluetoothDeviceClient();
  virtual ~MockBluetoothDeviceClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(GetProperties, Properties*(const dbus::ObjectPath&));
  MOCK_METHOD3(DiscoverServices, void(const dbus::ObjectPath&,
                                      const std::string&,
                                      const ServicesCallback&));
  MOCK_METHOD2(CancelDiscovery, void(const dbus::ObjectPath&,
                                     const DeviceCallback&));
  MOCK_METHOD2(Disconnect, void(const dbus::ObjectPath&,
                                const DeviceCallback&));
  MOCK_METHOD3(CreateNode, void(const dbus::ObjectPath&,
                                const std::string&,
                                const NodeCallback&));
  MOCK_METHOD3(RemoveNode, void(const dbus::ObjectPath&,
                                const dbus::ObjectPath&,
                                const NodeCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_BLUETOOTH_DEVICE_CLIENT_H_
