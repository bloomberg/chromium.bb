// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_test.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::_;

namespace policy {

const char kTestChromeSchema[] =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"StringPolicy\": { \"type\": \"string\" },"
    "    \"BooleanPolicy\": { \"type\": \"boolean\" },"
    "    \"IntegerPolicy\": { \"type\": \"integer\" },"
    "    \"StringListPolicy\": {"
    "      \"type\": \"array\","
    "      \"items\": { \"type\": \"string\" }"
    "    },"
    "    \"DictionaryPolicy\": {"
    "      \"type\": \"object\","
    "      \"properties\": {"
    "        \"bool\": { \"type\": \"boolean\" },"
    "        \"double\": { \"type\": \"number\" },"
    "        \"int\": { \"type\": \"integer\" },"
    "        \"string\": { \"type\": \"string\" },"
    "        \"array\": {"
    "          \"type\": \"array\","
    "          \"items\": { \"type\": \"string\" }"
    "        },"
    "        \"dictionary\": {"
    "          \"type\": \"object\","
    "          \"properties\": {"
    "            \"sub\": { \"type\": \"string\" },"
    "            \"sublist\": {"
    "              \"type\": \"array\","
    "              \"items\": {"
    "                \"type\": \"object\","
    "                \"properties\": {"
    "                  \"aaa\": { \"type\": \"integer\" },"
    "                  \"bbb\": { \"type\": \"integer\" },"
    "                  \"ccc\": { \"type\": \"string\" },"
    "                  \"ddd\": { \"type\": \"string\" }"
    "                }"
    "              }"
    "            }"
    "          }"
    "        },"
    "        \"list\": {"
    "          \"type\": \"array\","
    "          \"items\": {"
    "            \"type\": \"object\","
    "            \"properties\": {"
    "              \"subdictindex\": { \"type\": \"integer\" },"
    "              \"subdict\": {"
    "                \"type\": \"object\","
    "                \"properties\": {"
    "                  \"bool\": { \"type\": \"boolean\" },"
    "                  \"double\": { \"type\": \"number\" },"
    "                  \"int\": { \"type\": \"integer\" },"
    "                  \"string\": { \"type\": \"string\" }"
    "                }"
    "              }"
    "            }"
    "          }"
    "        },"
    "        \"dict\": {"
    "          \"type\": \"object\","
    "          \"properties\": {"
    "            \"bool\": { \"type\": \"boolean\" },"
    "            \"double\": { \"type\": \"number\" },"
    "            \"int\": { \"type\": \"integer\" },"
    "            \"string\": { \"type\": \"string\" },"
    "            \"list\": {"
    "              \"type\": \"array\","
    "              \"items\": {"
    "                \"type\": \"object\","
    "                \"properties\": {"
    "                  \"subdictindex\": { \"type\": \"integer\" },"
    "                  \"subdict\": {"
    "                    \"type\": \"object\","
    "                    \"properties\": {"
    "                      \"bool\": { \"type\": \"boolean\" },"
    "                      \"double\": { \"type\": \"number\" },"
    "                      \"int\": { \"type\": \"integer\" },"
    "                      \"string\": { \"type\": \"string\" }"
    "                    }"
    "                  }"
    "                }"
    "              }"
    "            }"
    "          }"
    "        }"
    "      }"
    "    }"
    "  }"
    "}";

namespace test_keys {

const char kKeyString[] = "StringPolicy";
const char kKeyBoolean[] = "BooleanPolicy";
const char kKeyInteger[] = "IntegerPolicy";
const char kKeyStringList[] = "StringListPolicy";
const char kKeyDictionary[] = "DictionaryPolicy";

}  // namespace test_keys

PolicyTestBase::PolicyTestBase() {}

PolicyTestBase::~PolicyTestBase() {}

void PolicyTestBase::SetUp() {
  std::string error;
  chrome_schema_ = Schema::Parse(kTestChromeSchema, &error);
  ASSERT_TRUE(chrome_schema_.valid()) << error;
  schema_registry_.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_CHROME, ""),
                                     chrome_schema_);
}

void PolicyTestBase::TearDown() {
  loop_.RunUntilIdle();
}

PolicyProviderTestHarness::PolicyProviderTestHarness(PolicyLevel level,
                                                     PolicyScope scope)
    : level_(level), scope_(scope) {}

PolicyProviderTestHarness::~PolicyProviderTestHarness() {}

PolicyLevel PolicyProviderTestHarness::policy_level() const {
  return level_;
}

PolicyScope PolicyProviderTestHarness::policy_scope() const {
  return scope_;
}

void PolicyProviderTestHarness::Install3rdPartyPolicy(
    const base::DictionaryValue* policies) {
  FAIL();
}

ConfigurationPolicyProviderTest::ConfigurationPolicyProviderTest() {}

ConfigurationPolicyProviderTest::~ConfigurationPolicyProviderTest() {}

void ConfigurationPolicyProviderTest::SetUp() {
  PolicyTestBase::SetUp();

  test_harness_.reset((*GetParam())());
  test_harness_->SetUp();

  Schema extension_schema =
      chrome_schema_.GetKnownProperty(test_keys::kKeyDictionary);
  ASSERT_TRUE(extension_schema.valid());
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
      extension_schema);
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
      extension_schema);
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                      "cccccccccccccccccccccccccccccccc"),
      extension_schema);

  provider_.reset(test_harness_->CreateProvider(&schema_registry_,
                                                loop_.message_loop_proxy()));
  provider_->Init(&schema_registry_);
  // Some providers do a reload on init. Make sure any notifications generated
  // are fired now.
  loop_.RunUntilIdle();

  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_->policies().Equals(kEmptyBundle));
}

void ConfigurationPolicyProviderTest::TearDown() {
  // Give providers the chance to clean up after themselves on the file thread.
  provider_->Shutdown();
  provider_.reset();

  PolicyTestBase::TearDown();
}

void ConfigurationPolicyProviderTest::CheckValue(
    const char* policy_name,
    const base::Value& expected_value,
    base::Closure install_value) {
  // Install the value, reload policy and check the provider for the value.
  install_value.Run();
  provider_->RefreshPolicies();
  loop_.RunUntilIdle();
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(policy_name,
           test_harness_->policy_level(),
           test_harness_->policy_scope(),
           expected_value.DeepCopy(),
           NULL);
  EXPECT_TRUE(provider_->policies().Equals(expected_bundle));
  // TODO(joaodasilva): set the policy in the POLICY_DOMAIN_EXTENSIONS too,
  // and extend the |expected_bundle|, once all providers are ready.
}

TEST_P(ConfigurationPolicyProviderTest, Empty) {
  provider_->RefreshPolicies();
  loop_.RunUntilIdle();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_->policies().Equals(kEmptyBundle));
}

TEST_P(ConfigurationPolicyProviderTest, StringValue) {
  const char kTestString[] = "string_value";
  base::StringValue expected_value(kTestString);
  CheckValue(test_keys::kKeyString,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallStringPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyString,
                        kTestString));
}

TEST_P(ConfigurationPolicyProviderTest, BooleanValue) {
  base::FundamentalValue expected_value(true);
  CheckValue(test_keys::kKeyBoolean,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallBooleanPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyBoolean,
                        true));
}

TEST_P(ConfigurationPolicyProviderTest, IntegerValue) {
  base::FundamentalValue expected_value(42);
  CheckValue(test_keys::kKeyInteger,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallIntegerPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyInteger,
                        42));
}

TEST_P(ConfigurationPolicyProviderTest, StringListValue) {
  base::ListValue expected_value;
  expected_value.Set(0U, base::Value::CreateStringValue("first"));
  expected_value.Set(1U, base::Value::CreateStringValue("second"));
  CheckValue(test_keys::kKeyStringList,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallStringListPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyStringList,
                        &expected_value));
}

TEST_P(ConfigurationPolicyProviderTest, DictionaryValue) {
  base::DictionaryValue expected_value;
  expected_value.SetBoolean("bool", true);
  expected_value.SetDouble("double", 123.456);
  expected_value.SetInteger("int", 123);
  expected_value.SetString("string", "omg");

  base::ListValue* list = new base::ListValue();
  list->Set(0U, base::Value::CreateStringValue("first"));
  list->Set(1U, base::Value::CreateStringValue("second"));
  expected_value.Set("array", list);

  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("sub", "value");
  list = new base::ListValue();
  base::DictionaryValue* sub = new base::DictionaryValue();
  sub->SetInteger("aaa", 111);
  sub->SetInteger("bbb", 222);
  list->Append(sub);
  sub = new base::DictionaryValue();
  sub->SetString("ccc", "333");
  sub->SetString("ddd", "444");
  list->Append(sub);
  dict->Set("sublist", list);
  expected_value.Set("dictionary", dict);

  CheckValue(test_keys::kKeyDictionary,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallDictionaryPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyDictionary,
                        &expected_value));
}

TEST_P(ConfigurationPolicyProviderTest, RefreshPolicies) {
  PolicyBundle bundle;
  EXPECT_TRUE(provider_->policies().Equals(bundle));

  // OnUpdatePolicy is called even when there are no changes.
  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(provider_->policies().Equals(bundle));

  // OnUpdatePolicy is called when there are changes.
  test_harness_->InstallStringPolicy(test_keys::kKeyString, "value");
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_keys::kKeyString,
           test_harness_->policy_level(),
           test_harness_->policy_scope(),
           base::Value::CreateStringValue("value"),
           NULL);
  EXPECT_TRUE(provider_->policies().Equals(bundle));
  provider_->RemoveObserver(&observer);
}

TEST(ConfigurationPolicyProviderTest, FixDeprecatedPolicies) {
  PolicyMap policy_map;
  policy_map.Set(key::kProxyServerMode,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 base::Value::CreateIntegerValue(3),
                 NULL);

  // Both these policies should be ignored, since there's a higher priority
  // policy available.
  policy_map.Set(key::kProxyMode,
                 POLICY_LEVEL_RECOMMENDED,
                 POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("pac_script"),
                 NULL);
  policy_map.Set(key::kProxyPacUrl,
                 POLICY_LEVEL_RECOMMENDED,
                 POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("http://example.com/wpad.dat"),
                 NULL);

  MockConfigurationPolicyProvider provider;
  provider.Init();
  provider.UpdateChromePolicy(policy_map);

  PolicyBundle expected_bundle;
  base::DictionaryValue* expected_value = new base::DictionaryValue();
  expected_value->SetInteger(key::kProxyServerMode, 3);
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(key::kProxySettings,
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_USER,
           expected_value,
           NULL);
  EXPECT_TRUE(provider.policies().Equals(expected_bundle));
  provider.Shutdown();
}

Configuration3rdPartyPolicyProviderTest::
    Configuration3rdPartyPolicyProviderTest() {}

Configuration3rdPartyPolicyProviderTest::
    ~Configuration3rdPartyPolicyProviderTest() {}

TEST_P(Configuration3rdPartyPolicyProviderTest, Load3rdParty) {
  base::DictionaryValue policy_dict;
  policy_dict.SetBoolean("bool", true);
  policy_dict.SetDouble("double", 123.456);
  policy_dict.SetInteger("int", 789);
  policy_dict.SetString("string", "string value");

  base::ListValue* list = new base::ListValue();
  for (int i = 0; i < 2; ++i) {
    base::DictionaryValue* dict = new base::DictionaryValue();
    dict->SetInteger("subdictindex", i);
    dict->Set("subdict", policy_dict.DeepCopy());
    list->Append(dict);
  }
  policy_dict.Set("list", list);
  policy_dict.Set("dict", policy_dict.DeepCopy());

  // Install these policies as a Chrome policy.
  test_harness_->InstallDictionaryPolicy(test_keys::kKeyDictionary,
                                         &policy_dict);
  // Install them as 3rd party policies too.
  base::DictionaryValue policy_3rdparty;
  policy_3rdparty.Set("extensions.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                      policy_dict.DeepCopy());
  policy_3rdparty.Set("extensions.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                      policy_dict.DeepCopy());
  // Install invalid 3rd party policies that shouldn't be loaded. These also
  // help detecting memory leaks in the code paths that detect invalid input.
  policy_3rdparty.Set("invalid-domain.component", policy_dict.DeepCopy());
  policy_3rdparty.Set("extensions.cccccccccccccccccccccccccccccccc",
                      base::Value::CreateStringValue("invalid-value"));
  test_harness_->Install3rdPartyPolicy(&policy_3rdparty);

  provider_->RefreshPolicies();
  loop_.RunUntilIdle();

  PolicyMap expected_policy;
  expected_policy.Set(test_keys::kKeyDictionary,
                      test_harness_->policy_level(),
                      test_harness_->policy_scope(),
                      policy_dict.DeepCopy(),
                      NULL);
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .CopyFrom(expected_policy);
  expected_policy.Clear();
  expected_policy.LoadFrom(&policy_dict,
                           test_harness_->policy_level(),
                           test_harness_->policy_scope());
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))
      .CopyFrom(expected_policy);
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"))
      .CopyFrom(expected_policy);
  EXPECT_TRUE(provider_->policies().Equals(expected_bundle));
}

}  // namespace policy
