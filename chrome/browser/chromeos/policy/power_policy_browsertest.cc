// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_power_manager_client.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "chromeos/dbus/mock_update_engine_client.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

namespace {

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::Return;
using ::testing::_;

namespace pm = power_manager;

MATCHER_P(PowerManagementPolicyMatches, expected_power_management_policy,
          std::string(negation ? "matches" : "does not match") +
              "the expected power management policy") {
  // A power management policy without an |ac_delays| message is equivalent to
  // a power management policy with an empty |ac_delays| message but their
  // serializations are different. To simplify the equality check, ensure that
  // the expected and actual power management policies both have an |ac_delays|
  // message. The same applies to the |battery_delays| message.
  pm::PowerManagementPolicy expected(expected_power_management_policy);
  expected.mutable_ac_delays();
  expected.mutable_battery_delays();
  pm::PowerManagementPolicy actual(arg);
  actual.mutable_ac_delays();
  actual.mutable_battery_delays();
  actual.clear_reason();
  return actual.SerializeAsString() == expected.SerializeAsString();
}

}  // namespace

class PowerPolicyBrowserTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  void SetUserPolicyAndVerifyPowerManagementPolicy(
      const std::string& user_policy_name,
      base::Value* user_policy_value,
      const pm::PowerManagementPolicy& power_management_policy);

  chromeos::MockPowerManagerClient* power_manager_client_;

 private:
  MockConfigurationPolicyProvider provider_;
};

void PowerPolicyBrowserTest::SetUpInProcessBrowserTestFixture() {
  chromeos::MockDBusThreadManager* dbus_thread_manager =
      new chromeos::MockDBusThreadManager;
  chromeos::DBusThreadManager::InitializeForTesting(dbus_thread_manager);
  EXPECT_CALL(*dbus_thread_manager->mock_session_manager_client(),
              RetrieveUserPolicy(_));
  EXPECT_CALL(*dbus_thread_manager->mock_update_engine_client(),
              GetLastStatus())
      .Times(1)
      .WillOnce(Return(chromeos::MockUpdateEngineClient::Status()));
  power_manager_client_ = dbus_thread_manager->mock_power_manager_client();
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider_, RegisterPolicyDomain(_, _)).Times(AnyNumber());
  BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

// Verify that setting |user_policy_name| to |user_policy_value| causes the
// updated |power_management_policy| to be sent to the power management backend.
void PowerPolicyBrowserTest::SetUserPolicyAndVerifyPowerManagementPolicy(
    const std::string& user_policy_name,
    base::Value* user_policy_value,
    const pm::PowerManagementPolicy& power_management_policy) {
  // Setting the user policy causes a notification which triggers an update to
  // the backend. If other policies had been set before, they will get unset,
  // causing additional notifications and triggering duplicate updates to the
  // backend. Hence, expect one or more updates.
  EXPECT_CALL(*power_manager_client_,
              SetPolicy(PowerManagementPolicyMatches(power_management_policy)))
      .Times(AtLeast(1));
  PolicyMap policy_map;
  policy_map.Set(user_policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 user_policy_value);
  provider_.UpdateChromePolicy(policy_map);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(power_manager_client_);
}

IN_PROC_BROWSER_TEST_F(PowerPolicyBrowserTest, SetPowerPolicy) {
  pm::PowerManagementPolicy power_management_policy;

  power_management_policy.set_idle_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kIdleAction,
      base::Value::CreateIntegerValue(pm::PowerManagementPolicy::STOP_SESSION),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.set_lid_closed_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kLidCloseAction,
      base::Value::CreateIntegerValue(pm::PowerManagementPolicy::STOP_SESSION),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.mutable_ac_delays()->set_idle_ms(900000);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kIdleDelayAC,
      base::Value::CreateIntegerValue(900000),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.mutable_ac_delays()->set_screen_off_ms(900000);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kScreenOffDelayAC,
      base::Value::CreateIntegerValue(900000),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.mutable_ac_delays()->set_screen_dim_ms(900000);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kScreenDimDelayAC,
      base::Value::CreateIntegerValue(900000),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.mutable_ac_delays()->set_screen_lock_ms(900000);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kScreenLockDelayAC,
      base::Value::CreateIntegerValue(900000),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.mutable_battery_delays()->set_idle_ms(900000);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kIdleDelayBattery,
      base::Value::CreateIntegerValue(900000),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.mutable_battery_delays()->set_screen_off_ms(900000);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kScreenOffDelayBattery,
      base::Value::CreateIntegerValue(900000),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.mutable_battery_delays()->set_screen_dim_ms(900000);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kScreenDimDelayBattery,
      base::Value::CreateIntegerValue(900000),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.mutable_battery_delays()->set_screen_lock_ms(900000);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kScreenLockDelayBattery,
      base::Value::CreateIntegerValue(900000),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.set_use_audio_activity(false);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kPowerManagementUsesAudioActivity,
      base::Value::CreateBooleanValue(false),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.set_use_video_activity(false);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kPowerManagementUsesVideoActivity,
      base::Value::CreateBooleanValue(false),
      power_management_policy);

  power_management_policy.Clear();
  power_management_policy.set_presentation_idle_delay_factor(3.0);
  SetUserPolicyAndVerifyPowerManagementPolicy(
      key::kPresentationIdleDelayScale,
      base::Value::CreateIntegerValue(300),
      power_management_policy);

  // During teardown, an empty power management policy should be sent to the
  // backend.
  power_management_policy.Clear();
  EXPECT_CALL(*power_manager_client_, SetPolicy(
      PowerManagementPolicyMatches(power_management_policy)));
  EXPECT_CALL(*power_manager_client_, RemoveObserver(_)).Times(AnyNumber());
}

}  // namespace policy
