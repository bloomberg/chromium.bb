// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_EXPERIMENTAL_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_EXPERIMENTAL_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_

#include <string>

#include "chromeos/dbus/experimental_bluetooth_profile_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockExperimentalBluetoothProfileManagerClient
    : public ExperimentalBluetoothProfileManagerClient {
 public:
  MockExperimentalBluetoothProfileManagerClient();
  virtual ~MockExperimentalBluetoothProfileManagerClient();

  MOCK_METHOD5(RegisterProfile, void(const dbus::ObjectPath&,
                                     const std::string&,
                                     const Options&,
                                     const base::Closure&,
                                     const ErrorCallback&));
  MOCK_METHOD3(UnregisterProfile, void(const dbus::ObjectPath&,
                                       const base::Closure&,
                                       const ErrorCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_EXPERIMENTAL_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
