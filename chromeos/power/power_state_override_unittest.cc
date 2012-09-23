// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/power/power_state_override.h"

#include "base/message_loop.h"
#include "chromeos/dbus/mock_power_manager_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::SaveArg;

namespace chromeos {

class PowerStateOverrideTest : public testing::Test {
 protected:
  virtual void SetUp() {
    MockDBusThreadManager* dbus_manager = new MockDBusThreadManager;
    DBusThreadManager::InitializeForTesting(dbus_manager);
    power_manager_client_ = dbus_manager->mock_power_manager_client();
  }

  virtual void TearDown() {
    DBusThreadManager::Shutdown();
  }

 protected:
  // Needed for PowerStateOverride's timer.
  MessageLoop message_loop_;

  MockPowerManagerClient* power_manager_client_;  // not owned
};

TEST_F(PowerStateOverrideTest, AddAndRemoveOverrides) {
  // An arbitrary ID to return in response to a request with ID 0.
  const uint32 kRequestId = 10;

  // Override bitmaps corresponding to different modes.
  const uint32 kDisplayOverrides =
      PowerManagerClient::DISABLE_IDLE_DIM |
      PowerManagerClient::DISABLE_IDLE_BLANK |
      PowerManagerClient::DISABLE_IDLE_SUSPEND |
      PowerManagerClient::DISABLE_IDLE_LID_SUSPEND;
  const uint32 kSystemOverrides =
      PowerManagerClient::DISABLE_IDLE_SUSPEND |
      PowerManagerClient::DISABLE_IDLE_LID_SUSPEND;

  // Block display sleep and pass a request ID to the callback.
  chromeos::PowerStateRequestIdCallback request_id_callback;
  EXPECT_CALL(*power_manager_client_,
              RequestPowerStateOverrides(0, _, kDisplayOverrides, _))
      .WillOnce(SaveArg<3>(&request_id_callback));
  scoped_ptr<PowerStateOverride> override(
      new PowerStateOverride(PowerStateOverride::BLOCK_DISPLAY_SLEEP));
  request_id_callback.Run(kRequestId);

  // The request should be canceled when the PowerStateOverride is destroyed.
  EXPECT_CALL(*power_manager_client_, CancelPowerStateOverrides(kRequestId));
  override.reset();

  // Now send a request to just block the system from suspending.
  Mock::VerifyAndClearExpectations(power_manager_client_);
  EXPECT_CALL(*power_manager_client_,
              RequestPowerStateOverrides(0, _, kSystemOverrides, _))
      .WillOnce(SaveArg<3>(&request_id_callback));
  override.reset(
      new PowerStateOverride(PowerStateOverride::BLOCK_SYSTEM_SUSPEND));
  request_id_callback.Run(kRequestId);
  EXPECT_CALL(*power_manager_client_, CancelPowerStateOverrides(kRequestId));
  override.reset();
}

}  // namespace chromeos
