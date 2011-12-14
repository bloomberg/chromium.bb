// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MOCK_POWER_MANAGER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MOCK_POWER_MANAGER_CLIENT_H_

#include <string>

#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockPowerManagerClient : public PowerManagerClient {
 public:
  MockPowerManagerClient();
  virtual ~MockPowerManagerClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(DecreaseScreenBrightness, void(bool));
  MOCK_METHOD0(IncreaseScreenBrightness, void(void));
  MOCK_METHOD0(RequestStatusUpdate, void(void));
  MOCK_METHOD0(RequestRestart, void(void));
  MOCK_METHOD0(RequestShutdown, void(void));
  MOCK_METHOD1(CalculateIdleTime, void(const CalculateIdleTimeCallback&));
  MOCK_METHOD0(NotifyScreenLockRequested, void(void));
  MOCK_METHOD0(NotifyScreenLockCompleted, void(void));
  MOCK_METHOD0(NotifyScreenUnlockRequested, void(void));
  MOCK_METHOD0(NotifyScreenUnlockCompleted, void(void));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MOCK_POWER_MANAGER_CLIENT_H_
