// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_EXPERIMENTAL_BLUETOOTH_AGENT_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_EXPERIMENTAL_BLUETOOTH_AGENT_MANAGER_CLIENT_H_

#include <string>

#include "chromeos/dbus/experimental_bluetooth_agent_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockExperimentalBluetoothAgentManagerClient
    : public ExperimentalBluetoothAgentManagerClient {
 public:
  MockExperimentalBluetoothAgentManagerClient();
  virtual ~MockExperimentalBluetoothAgentManagerClient();

  MOCK_METHOD4(RegisterAgent, void(const dbus::ObjectPath&,
                                   const std::string&,
                                   const base::Closure&,
                                   const ErrorCallback&));
  MOCK_METHOD3(UnregisterAgent, void(const dbus::ObjectPath&,
                                     const base::Closure&,
                                     const ErrorCallback&));
  MOCK_METHOD3(RequestDefaultAgent, void(const dbus::ObjectPath&,
                                         const base::Closure&,
                                         const ErrorCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_EXPERIMENTAL_BLUETOOTH_AGENT_MANAGER_CLIENT_H_
