// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_ROOT_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_ROOT_POWER_MANAGER_CLIENT_H_

#include "chromeos/dbus/root_power_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockRootPowerManagerClient : public RootPowerManagerClient {
 public:
  MockRootPowerManagerClient();
  virtual ~MockRootPowerManagerClient();

  MOCK_METHOD1(AddObserver, void(RootPowerManagerObserver*));
  MOCK_METHOD1(RemoveObserver, void(RootPowerManagerObserver*));
  MOCK_METHOD1(HasObserver, bool(RootPowerManagerObserver*));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_ROOT_POWER_MANAGER_CLIENT_H_
