// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_POWER_MANAGER_CLIENT_H_

#include <string>

#include "chromeos/dbus/power_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockPowerManagerClient : public PowerManagerClient {
 public:
  MockPowerManagerClient();
  virtual ~MockPowerManagerClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(HasObserver, bool(Observer*));
  MOCK_METHOD1(DecreaseScreenBrightness, void(bool));
  MOCK_METHOD0(IncreaseScreenBrightness, void(void));
  MOCK_METHOD2(SetScreenBrightnessPercent, void(double, bool));
  MOCK_METHOD1(GetScreenBrightnessPercent,
               void(const GetScreenBrightnessPercentCallback&));
  MOCK_METHOD1(RequestStatusUpdate, void(UpdateRequestType));
  MOCK_METHOD0(RequestRestart, void(void));
  MOCK_METHOD0(RequestShutdown, void(void));
  MOCK_METHOD1(CalculateIdleTime, void(const CalculateIdleTimeCallback&));
  MOCK_METHOD1(RequestIdleNotification, void(int64));
  MOCK_METHOD0(RequestActiveNotification, void(void));
  MOCK_METHOD4(RequestPowerStateOverrides,
               void(uint32,
                    uint32,
                    int,
                    const PowerStateRequestIdCallback&));
  MOCK_METHOD0(NotifyScreenLockRequested, void(void));
  MOCK_METHOD0(NotifyScreenLockCompleted, void(void));
  MOCK_METHOD0(NotifyScreenUnlockRequested, void(void));
  MOCK_METHOD0(NotifyScreenUnlockCompleted, void(void));
  MOCK_METHOD0(GetIsScreenLocked, bool(void));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_POWER_MANAGER_CLIENT_H_
