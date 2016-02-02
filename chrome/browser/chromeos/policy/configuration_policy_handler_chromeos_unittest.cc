// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/prefs/pref_value_map.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Test cases for the screen magnifier type policy setting.
class ScreenMagnifierPolicyHandlerTest : public testing::Test {
 protected:
  PolicyMap policy_;
  PrefValueMap prefs_;
  ScreenMagnifierPolicyHandler handler_;
};

class LoginScreenPowerManagementPolicyHandlerTest : public testing::Test {
 protected:
  LoginScreenPowerManagementPolicyHandlerTest();

  void SetUp() override;

  Schema chrome_schema_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenPowerManagementPolicyHandlerTest);
};

LoginScreenPowerManagementPolicyHandlerTest::
    LoginScreenPowerManagementPolicyHandlerTest() {
}

void LoginScreenPowerManagementPolicyHandlerTest::SetUp() {
  chrome_schema_ = Schema::Wrap(GetChromeSchemaData());
}

TEST_F(ScreenMagnifierPolicyHandlerTest, Default) {
  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_FALSE(
      prefs_.GetValue(prefs::kAccessibilityScreenMagnifierEnabled, NULL));
  EXPECT_FALSE(prefs_.GetValue(prefs::kAccessibilityScreenMagnifierType, NULL));
}

TEST_F(ScreenMagnifierPolicyHandlerTest, Disabled) {
  policy_.Set(key::kScreenMagnifierType,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD,
              new base::FundamentalValue(0),
              NULL);
  handler_.ApplyPolicySettings(policy_, &prefs_);

  const base::Value* enabled = NULL;
  EXPECT_TRUE(
      prefs_.GetValue(prefs::kAccessibilityScreenMagnifierEnabled, &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_TRUE(base::FundamentalValue(false).Equals(enabled));

  const base::Value* type = NULL;
  EXPECT_TRUE(prefs_.GetValue(prefs::kAccessibilityScreenMagnifierType, &type));
  ASSERT_TRUE(type);
  EXPECT_TRUE(base::FundamentalValue(0).Equals(type));
}

TEST_F(ScreenMagnifierPolicyHandlerTest, Enabled) {
  policy_.Set(key::kScreenMagnifierType,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD,
              new base::FundamentalValue(1),
              NULL);
  handler_.ApplyPolicySettings(policy_, &prefs_);

  const base::Value* enabled = NULL;
  EXPECT_TRUE(
      prefs_.GetValue(prefs::kAccessibilityScreenMagnifierEnabled, &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_TRUE(base::FundamentalValue(true).Equals(enabled));

  const base::Value* type = NULL;
  EXPECT_TRUE(prefs_.GetValue(prefs::kAccessibilityScreenMagnifierType, &type));
  ASSERT_TRUE(type);
  EXPECT_TRUE(base::FundamentalValue(1).Equals(type));
}

TEST(ExternalDataPolicyHandlerTest, Empty) {
  PolicyErrorMap errors;
  EXPECT_TRUE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                  .CheckPolicySettings(PolicyMap(), &errors));
  EXPECT_TRUE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, WrongType) {
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD,
                 new base::FundamentalValue(false),
                 NULL);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, MissingURL) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("hash", "1234567890123456789012345678901234567890");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD,
                 dict.release(),
                 NULL);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, InvalidURL) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("url", "http://");
  dict->SetString("hash", "1234567890123456789012345678901234567890");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD,
                 dict.release(),
                 NULL);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, MissingHash) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("url", "http://localhost/");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD,
                 dict.release(),
                 NULL);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, InvalidHash) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("url", "http://localhost/");
  dict->SetString("hash", "1234");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD,
                 dict.release(),
                 NULL);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, Valid) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("url", "http://localhost/");
  dict->SetString(
      "hash",
      "1234567890123456789012345678901234567890123456789012345678901234");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD,
                 dict.release(),
                 NULL);
  PolicyErrorMap errors;
  EXPECT_TRUE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                  .CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kUserAvatarImage).empty());
}

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
                 POLICY_SOURCE_CLOUD,
                 new base::StringValue(kTestONC),
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
                 POLICY_SOURCE_CLOUD,
                 new base::FundamentalValue(false),
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
                 POLICY_SOURCE_CLOUD,
                 new base::StringValue(kTestONC),
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
                 POLICY_SOURCE_CLOUD,
                 new base::StringValue(kTestONC),
                 NULL);
  scoped_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  handler->PrepareForDisplaying(&policy_map);
  const base::Value* sanitized =
      policy_map.GetValue(key::kOpenNetworkConfiguration);
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
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, list.DeepCopy(),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(prefs::kPolicyPinnedLauncherApps, &value));
  EXPECT_TRUE(base::Value::Equals(&expected_pinned_apps, value));

  base::StringValue entry1("abcdefghijklmnopabcdefghijklmnop");
  base::DictionaryValue* entry1_dict = new base::DictionaryValue();
  entry1_dict->Set(ash::kPinnedAppsPrefAppIDPath, entry1.DeepCopy());
  expected_pinned_apps.Append(entry1_dict);
  list.Append(entry1.DeepCopy());
  policy_map.Set(key::kPinnedLauncherApps, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, list.DeepCopy(),
                 nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(prefs::kPolicyPinnedLauncherApps, &value));
  EXPECT_TRUE(base::Value::Equals(&expected_pinned_apps, value));
}

TEST_F(LoginScreenPowerManagementPolicyHandlerTest, Empty) {
  PolicyMap policy_map;
  LoginScreenPowerManagementPolicyHandler handler(chrome_schema_);
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

TEST_F(LoginScreenPowerManagementPolicyHandlerTest, ValidPolicy) {
  PolicyMap policy_map;
  policy_map.Set(
      key::kDeviceLoginScreenPowerManagement, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kLoginScreenPowerManagementPolicy).release(),
      NULL);
  LoginScreenPowerManagementPolicyHandler handler(chrome_schema_);
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

TEST_F(LoginScreenPowerManagementPolicyHandlerTest, WrongType) {
  PolicyMap policy_map;
  policy_map.Set(key::kDeviceLoginScreenPowerManagement,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD,
                 new base::FundamentalValue(false),
                 NULL);
  LoginScreenPowerManagementPolicyHandler handler(chrome_schema_);
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

}  // namespace policy
