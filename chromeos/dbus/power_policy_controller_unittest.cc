// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power_policy_controller.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_power_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace chromeos {

class PowerPolicyControllerTest : public testing::Test {
 public:
  PowerPolicyControllerTest() {}
  virtual ~PowerPolicyControllerTest() {}

  virtual void SetUp() OVERRIDE {
    dbus_manager_ = new MockDBusThreadManager;
    DBusThreadManager::InitializeForTesting(dbus_manager_);  // Takes ownership.
    power_client_ = dbus_manager_->mock_power_manager_client();
    EXPECT_CALL(*power_client_, SetPolicy(_))
        .WillRepeatedly(SaveArg<0>(&last_policy_));

    policy_controller_ = dbus_manager_->GetPowerPolicyController();

    // TODO(derat): Write what looks like it will be a ridiculously large
    // amount of code to register prefs so that UpdatePolicyFromPrefs() can
    // be tested.
  }

  virtual void TearDown() OVERRIDE {
    DBusThreadManager::Shutdown();
  }

 protected:
  MockDBusThreadManager* dbus_manager_;  // Not owned.
  MockPowerManagerClient* power_client_;  // Not owned.
  PowerPolicyController* policy_controller_;  // Not owned.

  power_manager::PowerManagementPolicy last_policy_;
};

TEST_F(PowerPolicyControllerTest, Blocks) {
  const char kSuspendBlockReason[] = "suspend";
  const int suspend_id =
      policy_controller_->AddSuspendBlock(kSuspendBlockReason);
  power_manager::PowerManagementPolicy expected_policy;
  expected_policy.set_idle_action(
      power_manager::PowerManagementPolicy_Action_DO_NOTHING);
  expected_policy.set_reason(kSuspendBlockReason);
  EXPECT_EQ(expected_policy.SerializeAsString(),
            last_policy_.SerializeAsString());

  const char kScreenBlockReason[] = "screen";
  const int screen_id = policy_controller_->AddScreenBlock(kScreenBlockReason);
  expected_policy.mutable_ac_delays()->set_screen_dim_ms(0);
  expected_policy.mutable_ac_delays()->set_screen_off_ms(0);
  expected_policy.mutable_battery_delays()->set_screen_dim_ms(0);
  expected_policy.mutable_battery_delays()->set_screen_off_ms(0);
  expected_policy.set_reason(
      std::string(kScreenBlockReason) + ", " + kSuspendBlockReason);
  EXPECT_EQ(expected_policy.SerializeAsString(),
            last_policy_.SerializeAsString());

  policy_controller_->RemoveBlock(suspend_id);
  expected_policy.set_reason(kScreenBlockReason);
  EXPECT_EQ(expected_policy.SerializeAsString(),
            last_policy_.SerializeAsString());

  policy_controller_->RemoveBlock(screen_id);
  expected_policy.Clear();
  EXPECT_EQ(expected_policy.SerializeAsString(),
            last_policy_.SerializeAsString());
}

}  // namespace chromeos
