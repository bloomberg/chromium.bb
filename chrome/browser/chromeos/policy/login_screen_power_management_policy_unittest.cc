// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/login_screen_power_management_policy.h"

#include "chromeos/dbus/power_policy_controller.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kLoginScreenPowerManagementPolicy[] =
    "{"
    "  \"AC\": {"
    "    \"Delays\": {"
    "      \"ScreenDim\": 5000,"
    "      \"ScreenOff\": 7000,"
    "      \"Idle\": 9000"
    "    },"
    "    \"IdleAction\": \"DoNothing\""
    "  },"
    "  \"Battery\": {"
    "    \"Delays\": {"
    "      \"ScreenDim\": 1000,"
    "      \"ScreenOff\": 3000,"
    "      \"Idle\": 4000"
    "    },"
    "    \"IdleAction\": \"DoNothing\""
    "  },"
    "  \"LidCloseAction\": \"DoNothing\","
    "  \"UserActivityScreenDimDelayScale\": 300"
    "}";

}  // namespace

TEST(LoginScreenPowerManagementPolicyTest, InvalidJSON) {
  PolicyErrorMap errors;
  LoginScreenPowerManagementPolicy policy;
  EXPECT_FALSE(policy.Init("Invalid JSON!", &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  EXPECT_FALSE(policy.GetScreenDimDelayAC());
  EXPECT_FALSE(policy.GetScreenOffDelayAC());
  EXPECT_FALSE(policy.GetIdleDelayAC());
  EXPECT_FALSE(policy.GetScreenDimDelayBattery());
  EXPECT_FALSE(policy.GetScreenOffDelayBattery());
  EXPECT_FALSE(policy.GetIdleDelayBattery());
  EXPECT_FALSE(policy.GetIdleActionAC());
  EXPECT_FALSE(policy.GetIdleActionBattery());
  EXPECT_FALSE(policy.GetLidCloseAction());
  EXPECT_FALSE(policy.GetUserActivityScreenDimDelayScale());
}

TEST(LoginScreenPowerManagementPolicyTest, InitInvalidValues) {
  PolicyErrorMap errors;
  scoped_ptr<LoginScreenPowerManagementPolicy> policy;
  policy.reset(new LoginScreenPowerManagementPolicy);
  EXPECT_TRUE(policy->Init("{ \"AC\": { \"Delays\": { \"ScreenDim\": -1 } } }",
                           &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->GetScreenDimDelayAC());

  errors.Clear();
  policy.reset(new LoginScreenPowerManagementPolicy);
  EXPECT_TRUE(policy->Init("{ \"AC\": { \"Delays\": { \"ScreenOff\": -1 } } }",
                           &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->GetScreenOffDelayAC());

  errors.Clear();
  policy.reset(new LoginScreenPowerManagementPolicy);
  EXPECT_TRUE(policy->Init("{ \"AC\": { \"Delays\": { \"Idle\": -1 } } }",
                           &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->GetIdleDelayAC());

  errors.Clear();
  policy.reset(new LoginScreenPowerManagementPolicy);
  EXPECT_TRUE(policy->Init(
      "{ \"Battery\": { \"Delays\": { \"ScreenDim\": -1 } } }", &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->GetScreenDimDelayBattery());
  errors.Clear();
  policy.reset(new LoginScreenPowerManagementPolicy);
  EXPECT_TRUE(policy->Init(
      "{ \"Battery\": { \"Delays\": { \"ScreenOff\": -1 } } }", &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->GetScreenOffDelayBattery());

  errors.Clear();
  policy.reset(new LoginScreenPowerManagementPolicy);
  EXPECT_TRUE(policy->Init("{ \"Battery\": { \"Delays\": { \"Idle\": -1 } } }",
                           &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->GetIdleDelayBattery());

  errors.Clear();
  EXPECT_TRUE(policy->Init("{ \"AC\": { \"IdleAction\": \"SignOut\" } }",
                           &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  EXPECT_FALSE(policy->GetIdleActionAC());

  errors.Clear();
  policy.reset(new LoginScreenPowerManagementPolicy);
  EXPECT_TRUE(policy->Init("{ \"Battery\": { \"IdleAction\": \"SignOut\" } }",
                           &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->GetIdleActionBattery());

  errors.Clear();
  policy.reset(new LoginScreenPowerManagementPolicy);
  EXPECT_TRUE(policy->Init("{ \"LidCloseAction\": \"SignOut\" }", &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->GetLidCloseAction());

  errors.Clear();
  policy.reset(new LoginScreenPowerManagementPolicy);
  EXPECT_TRUE(policy->Init("{ \"UserActivityScreenDimDelayScale\": 50 }",
                           &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->GetUserActivityScreenDimDelayScale());
}

TEST(LoginScreenPowerManagementPolicyTest, ValidJSON) {
  PolicyErrorMap errors;
  LoginScreenPowerManagementPolicy policy;
  EXPECT_TRUE(policy.Init(kLoginScreenPowerManagementPolicy, &errors));
  EXPECT_TRUE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
  ASSERT_TRUE(policy.GetScreenDimDelayAC());
  EXPECT_TRUE(base::FundamentalValue(5000).Equals(
      policy.GetScreenDimDelayAC()));
  ASSERT_TRUE(policy.GetScreenOffDelayAC());
  EXPECT_TRUE(base::FundamentalValue(7000).Equals(
      policy.GetScreenOffDelayAC()));
  ASSERT_TRUE(policy.GetIdleDelayAC());
  EXPECT_TRUE(base::FundamentalValue(9000).Equals(policy.GetIdleDelayAC()));
  ASSERT_TRUE(policy.GetScreenDimDelayBattery());
  EXPECT_TRUE(base::FundamentalValue(1000).Equals(
      policy.GetScreenDimDelayBattery()));
  ASSERT_TRUE(policy.GetScreenOffDelayBattery());
  EXPECT_TRUE(base::FundamentalValue(3000).Equals(
      policy.GetScreenOffDelayBattery()));
  ASSERT_TRUE(policy.GetIdleDelayBattery());
  EXPECT_TRUE(base::FundamentalValue(4000).Equals(
      policy.GetIdleDelayBattery()));
  ASSERT_TRUE(policy.GetIdleActionAC());
  EXPECT_TRUE(base::FundamentalValue(
      chromeos::PowerPolicyController::ACTION_DO_NOTHING).Equals(
          policy.GetIdleActionAC()));
  ASSERT_TRUE(policy.GetIdleActionBattery());
  EXPECT_TRUE(base::FundamentalValue(
      chromeos::PowerPolicyController::ACTION_DO_NOTHING).Equals(
          policy.GetIdleActionBattery()));
  ASSERT_TRUE(policy.GetLidCloseAction());
  EXPECT_TRUE(base::FundamentalValue(
      chromeos::PowerPolicyController::ACTION_DO_NOTHING).Equals(
          policy.GetLidCloseAction()));
  ASSERT_TRUE(policy.GetUserActivityScreenDimDelayScale());
  EXPECT_TRUE(base::FundamentalValue(300).Equals(
      policy.GetUserActivityScreenDimDelayScale()));
}

}  // namespace policy
