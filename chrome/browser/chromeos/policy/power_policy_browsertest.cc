// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/power/power_api_manager.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/common/extensions/api/power.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_power_manager_client.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "chromeos/dbus/mock_update_engine_client.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

namespace {

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace pm = power_manager;

}  // namespace

class PowerPolicyBrowserTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  // Sets |user_policy_name| to |user_policy_value|.
  void SetUserPolicy(const std::string& user_policy_name,
                     base::Value* user_policy_value);

  // Returns a string describing |policy|.
  std::string GetDebugString(const pm::PowerManagementPolicy& policy);

  chromeos::MockPowerManagerClient* power_manager_client_;

  // Last PowerManagementPolicy sent by |power_manager_client_|.
  pm::PowerManagementPolicy last_power_management_policy_;

 private:
  MockConfigurationPolicyProvider provider_;
};

void PowerPolicyBrowserTest::SetUpInProcessBrowserTestFixture() {
  chromeos::MockDBusThreadManager* dbus_thread_manager =
      new chromeos::MockDBusThreadManager;
  power_manager_client_ = dbus_thread_manager->mock_power_manager_client();

  // Capture the PowerManagementPolicy that's sent before tests start
  // making changes to the user policy.
  EXPECT_CALL(*power_manager_client_, SetPolicy(_))
      .WillRepeatedly(SaveArg<0>(&last_power_management_policy_));

  // Ignore uninteresting calls.
  EXPECT_CALL(*power_manager_client_, AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*power_manager_client_, RemoveObserver(_))
      .Times(AnyNumber());

  chromeos::DBusThreadManager::InitializeForTesting(dbus_thread_manager);
  EXPECT_CALL(*dbus_thread_manager->mock_session_manager_client(),
              RetrieveUserPolicy(_));
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider_, RegisterPolicyDomain(_, _)).Times(AnyNumber());
  BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void PowerPolicyBrowserTest::SetUserPolicy(
    const std::string& user_policy_name,
    base::Value* user_policy_value) {
  PolicyMap policy_map;
  policy_map.Set(user_policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 user_policy_value);
  provider_.UpdateChromePolicy(policy_map);
  base::RunLoop().RunUntilIdle();
}

std::string PowerPolicyBrowserTest::GetDebugString(
    const pm::PowerManagementPolicy& policy) {
  return chromeos::PowerPolicyController::GetPolicyDebugString(policy);
}

IN_PROC_BROWSER_TEST_F(PowerPolicyBrowserTest, SetPowerPolicy) {
  pm::PowerManagementPolicy original_power_management_policy =
      last_power_management_policy_;

  pm::PowerManagementPolicy power_management_policy =
      original_power_management_policy;
  power_management_policy.set_idle_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  SetUserPolicy(
      key::kIdleAction,
      base::Value::CreateIntegerValue(pm::PowerManagementPolicy::STOP_SESSION));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.set_lid_closed_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  SetUserPolicy(
      key::kLidCloseAction,
      base::Value::CreateIntegerValue(pm::PowerManagementPolicy::STOP_SESSION));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_ac_delays()->set_idle_ms(9000);
  SetUserPolicy(key::kIdleDelayAC, base::Value::CreateIntegerValue(9000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_ac_delays()->set_idle_warning_ms(8000);
  SetUserPolicy(key::kIdleWarningDelayAC,
                base::Value::CreateIntegerValue(8000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_ac_delays()->set_screen_off_ms(7000);
  SetUserPolicy(key::kScreenOffDelayAC, base::Value::CreateIntegerValue(7000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_ac_delays()->set_screen_dim_ms(5000);
  SetUserPolicy(key::kScreenDimDelayAC, base::Value::CreateIntegerValue(5000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_ac_delays()->set_screen_lock_ms(6000);
  SetUserPolicy(key::kScreenLockDelayAC, base::Value::CreateIntegerValue(6000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_battery_delays()->set_idle_ms(5000);
  SetUserPolicy(key::kIdleDelayBattery, base::Value::CreateIntegerValue(5000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_battery_delays()->set_idle_warning_ms(4000);
  SetUserPolicy(key::kIdleWarningDelayBattery,
                base::Value::CreateIntegerValue(4000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_battery_delays()->set_screen_off_ms(3000);
  SetUserPolicy(key::kScreenOffDelayBattery,
                base::Value::CreateIntegerValue(3000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_battery_delays()->set_screen_dim_ms(1000);
  SetUserPolicy(key::kScreenDimDelayBattery,
                base::Value::CreateIntegerValue(1000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.mutable_battery_delays()->set_screen_lock_ms(2000);
  SetUserPolicy(key::kScreenLockDelayBattery,
                base::Value::CreateIntegerValue(2000));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.set_use_audio_activity(false);
  SetUserPolicy(key::kPowerManagementUsesAudioActivity,
                base::Value::CreateBooleanValue(false));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.set_use_video_activity(false);
  SetUserPolicy(key::kPowerManagementUsesVideoActivity,
                base::Value::CreateBooleanValue(false));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));

  power_management_policy = original_power_management_policy;
  power_management_policy.set_presentation_idle_delay_factor(3.0);
  SetUserPolicy(key::kPresentationIdleDelayScale,
                base::Value::CreateIntegerValue(300));
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(last_power_management_policy_));
}

IN_PROC_BROWSER_TEST_F(PowerPolicyBrowserTest, AllowScreenWakeLocks) {
  pm::PowerManagementPolicy baseline_policy = last_power_management_policy_;

  // Default settings should have delays.
  pm::PowerManagementPolicy power_management_policy = baseline_policy;
  EXPECT_NE(0, baseline_policy.ac_delays().screen_dim_ms());
  EXPECT_NE(0, baseline_policy.ac_delays().screen_off_ms());
  EXPECT_NE(0, baseline_policy.battery_delays().screen_dim_ms());
  EXPECT_NE(0, baseline_policy.battery_delays().screen_off_ms());

  // Pretend an extension grabs a screen wake lock.
  const char kExtensionId[] = "abcdefghijklmnopabcdefghijlkmnop";
  extensions::PowerApiManager::GetInstance()->AddRequest(
      kExtensionId, extensions::api::power::LEVEL_DISPLAY);
  base::RunLoop().RunUntilIdle();

  // Check that the lock is in effect (ignoring idle_action and reason).
  pm::PowerManagementPolicy policy = baseline_policy;
  policy.mutable_ac_delays()->set_screen_dim_ms(0);
  policy.mutable_ac_delays()->set_screen_off_ms(0);
  policy.mutable_battery_delays()->set_screen_dim_ms(0);
  policy.mutable_battery_delays()->set_screen_off_ms(0);
  policy.set_idle_action(last_power_management_policy_.idle_action());
  policy.set_reason(last_power_management_policy_.reason());
  EXPECT_EQ(GetDebugString(policy),
            GetDebugString(last_power_management_policy_));

  // Engage the policy and verify that the defaults take effect again.
  SetUserPolicy(key::kAllowScreenWakeLocks,
                base::Value::CreateBooleanValue(false));
  policy = baseline_policy;
  policy.set_idle_action(last_power_management_policy_.idle_action());
  policy.set_reason(last_power_management_policy_.reason());
  EXPECT_EQ(GetDebugString(policy),
            GetDebugString(last_power_management_policy_));
}

}  // namespace policy
