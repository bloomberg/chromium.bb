// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"

#include "base/callback.h"
#include "base/prefs/pref_value_map.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/common/pref_names.h"
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

TEST(NetworkConfigurationPolicyHandlerTest, Empty) {
  PolicyMap policy_map;
  scoped_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  EXPECT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kOpenNetworkConfiguration).empty());
}

TEST(NetworkConfigurationPolicyHandlerTest, ValidONC) {
  const std::string kTestONC(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"Name\": \"some name\","
      "    \"WiFi\": {"
      "      \"Security\": \"WEP-PSK\","
      "      \"SSID\": \"ssid\","
      "      \"Passphrase\": \"pass\","
      "    }"
      "  }]"
      "}");

  PolicyMap policy_map;
  policy_map.Set(key::kOpenNetworkConfiguration,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 Value::CreateStringValue(kTestONC),
                 NULL);
  scoped_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  EXPECT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kOpenNetworkConfiguration).empty());
}

TEST(NetworkConfigurationPolicyHandlerTest, WrongType) {
  PolicyMap policy_map;
  policy_map.Set(key::kOpenNetworkConfiguration,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 Value::CreateBooleanValue(false),
                 NULL);
  scoped_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  EXPECT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kOpenNetworkConfiguration).empty());
}

TEST(NetworkConfigurationPolicyHandlerTest, JSONParseError) {
  const std::string kTestONC("I'm not proper JSON!");
  PolicyMap policy_map;
  policy_map.Set(key::kOpenNetworkConfiguration,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 Value::CreateStringValue(kTestONC),
                 NULL);
  scoped_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  EXPECT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kOpenNetworkConfiguration).empty());
}

TEST(NetworkConfigurationPolicyHandlerTest, Sanitization) {
  const std::string kTestONC(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"Name\": \"some name\","
      "    \"WiFi\": {"
      "      \"Security\": \"WEP-PSK\","
      "      \"SSID\": \"ssid\","
      "      \"Passphrase\": \"pass\","
      "    }"
      "  }]"
      "}");

  PolicyMap policy_map;
  policy_map.Set(key::kOpenNetworkConfiguration,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 Value::CreateStringValue(kTestONC),
                 NULL);
  scoped_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  handler->PrepareForDisplaying(&policy_map);
  const Value* sanitized = policy_map.GetValue(key::kOpenNetworkConfiguration);
  ASSERT_TRUE(sanitized);
  std::string sanitized_onc;
  EXPECT_TRUE(sanitized->GetAsString(&sanitized_onc));
  EXPECT_FALSE(sanitized_onc.empty());
  EXPECT_EQ(std::string::npos, sanitized_onc.find("pass"));
}

TEST(PinnedLauncherAppsPolicyHandler, PrefTranslation) {
  base::ListValue list;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::ListValue expected_pinned_apps;
  base::Value* value = NULL;
  PinnedLauncherAppsPolicyHandler handler;

  policy_map.Set(key::kPinnedLauncherApps, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(prefs::kPinnedLauncherApps, &value));
  EXPECT_TRUE(base::Value::Equals(&expected_pinned_apps, value));

  base::StringValue entry1("abcdefghijklmnopabcdefghijklmnop");
  base::DictionaryValue* entry1_dict = new base::DictionaryValue();
  entry1_dict->Set(ash::kPinnedAppsPrefAppIDPath, entry1.DeepCopy());
  expected_pinned_apps.Append(entry1_dict);
  list.Append(entry1.DeepCopy());
  policy_map.Set(key::kPinnedLauncherApps, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, list.DeepCopy(), NULL);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(prefs::kPinnedLauncherApps, &value));
  EXPECT_TRUE(base::Value::Equals(&expected_pinned_apps, value));
}

TEST(LoginScreenPowerManagementPolicyHandlerTest, Empty) {
  PolicyMap policy_map;
  LoginScreenPowerManagementPolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

TEST(LoginScreenPowerManagementPolicyHandlerTest, ValidPolicy) {
  PolicyMap policy_map;
  policy_map.Set(key::kDeviceLoginScreenPowerManagement,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 Value::CreateStringValue(kLoginScreenPowerManagementPolicy),
                 NULL);
  LoginScreenPowerManagementPolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

TEST(LoginScreenPowerManagementPolicyHandlerTest, WrongType) {
  PolicyMap policy_map;
  policy_map.Set(key::kDeviceLoginScreenPowerManagement,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 Value::CreateBooleanValue(false),
                 NULL);
  LoginScreenPowerManagementPolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

TEST(LoginScreenPowerManagementPolicyHandlerTest, JSONParseError) {
  const std::string policy("I'm not proper JSON!");
  PolicyMap policy_map;
  policy_map.Set(key::kDeviceLoginScreenPowerManagement,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 Value::CreateStringValue(policy),
                 NULL);
  LoginScreenPowerManagementPolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

}  // namespace policy
