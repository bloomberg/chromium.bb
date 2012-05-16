// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_test.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::_;

namespace policy {

namespace test_policy_definitions {

const char kKeyString[] = "StringPolicy";
const char kKeyBoolean[] = "BooleanPolicy";
const char kKeyInteger[] = "IntegerPolicy";
const char kKeyStringList[] = "StringListPolicy";
const char kKeyDictionary[] = "DictionaryPolicy";

static const PolicyDefinitionList::Entry kEntries[] = {
  { kKeyString,     base::Value::TYPE_STRING },
  { kKeyBoolean,    base::Value::TYPE_BOOLEAN },
  { kKeyInteger,    base::Value::TYPE_INTEGER },
  { kKeyStringList, base::Value::TYPE_LIST },
  { kKeyDictionary, base::Value::TYPE_DICTIONARY },
};

const PolicyDefinitionList kList = {
  kEntries, kEntries + arraysize(kEntries)
};

}  // namespace test_policy_definitions

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

ConfigurationPolicyProviderTest::ConfigurationPolicyProviderTest() {}

ConfigurationPolicyProviderTest::~ConfigurationPolicyProviderTest() {}

void ConfigurationPolicyProviderTest::SetUp() {
  AsynchronousPolicyTestBase::SetUp();

  test_harness_.reset((*GetParam())());
  test_harness_->SetUp();

  provider_.reset(
      test_harness_->CreateProvider(&test_policy_definitions::kList));
  // Some providers do a reload on init. Make sure any notifications generated
  // are fired now.
  loop_.RunAllPending();

  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_->policies().Equals(kEmptyBundle));
}

void ConfigurationPolicyProviderTest::TearDown() {
  // Give providers the chance to clean up after themselves on the file thread.
  provider_.reset();

  AsynchronousPolicyTestBase::TearDown();
}

void ConfigurationPolicyProviderTest::CheckValue(
    const char* policy_name,
    const base::Value& expected_value,
    base::Closure install_value) {
  // Install the value, reload policy and check the provider for the value.
  install_value.Run();
  provider_->RefreshPolicies();
  loop_.RunAllPending();
  PolicyBundle expected_bundle;
  expected_bundle.Get(POLICY_DOMAIN_CHROME, "")
      .Set(policy_name,
           test_harness_->policy_level(),
           test_harness_->policy_scope(),
           expected_value.DeepCopy());
  EXPECT_TRUE(provider_->policies().Equals(expected_bundle));
  // TODO(joaodasilva): set the policy in the POLICY_DOMAIN_EXTENSIONS too,
  // and extend the |expected_bundle|, once all providers are ready.
}

TEST_P(ConfigurationPolicyProviderTest, Empty) {
  provider_->RefreshPolicies();
  loop_.RunAllPending();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_->policies().Equals(kEmptyBundle));
}

TEST_P(ConfigurationPolicyProviderTest, StringValue) {
  const char kTestString[] = "string_value";
  base::StringValue expected_value(kTestString);
  CheckValue(test_policy_definitions::kKeyString,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallStringPolicy,
                        base::Unretained(test_harness_.get()),
                        test_policy_definitions::kKeyString,
                        kTestString));
}

TEST_P(ConfigurationPolicyProviderTest, BooleanValue) {
  base::FundamentalValue expected_value(true);
  CheckValue(test_policy_definitions::kKeyBoolean,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallBooleanPolicy,
                        base::Unretained(test_harness_.get()),
                        test_policy_definitions::kKeyBoolean,
                        true));
}

TEST_P(ConfigurationPolicyProviderTest, IntegerValue) {
  base::FundamentalValue expected_value(42);
  CheckValue(test_policy_definitions::kKeyInteger,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallIntegerPolicy,
                        base::Unretained(test_harness_.get()),
                        test_policy_definitions::kKeyInteger,
                        42));
}

TEST_P(ConfigurationPolicyProviderTest, StringListValue) {
  base::ListValue expected_value;
  expected_value.Set(0U, base::Value::CreateStringValue("first"));
  expected_value.Set(1U, base::Value::CreateStringValue("second"));
  CheckValue(test_policy_definitions::kKeyStringList,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallStringListPolicy,
                        base::Unretained(test_harness_.get()),
                        test_policy_definitions::kKeyStringList,
                        &expected_value));
}

TEST_P(ConfigurationPolicyProviderTest, DictionaryValue) {
  base::DictionaryValue expected_value;
  expected_value.SetBoolean("bool", true);
  expected_value.SetInteger("int", 123);
  expected_value.SetString("str", "omg");

  base::ListValue* list = new base::ListValue();
  list->Set(0U, base::Value::CreateStringValue("first"));
  list->Set(1U, base::Value::CreateStringValue("second"));
  expected_value.Set("list", list);

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
  expected_value.Set("dict", dict);

  CheckValue(test_policy_definitions::kKeyDictionary,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallDictionaryPolicy,
                        base::Unretained(test_harness_.get()),
                        test_policy_definitions::kKeyDictionary,
                        &expected_value));
}

TEST_P(ConfigurationPolicyProviderTest, RefreshPolicies) {
  PolicyBundle bundle;
  EXPECT_TRUE(provider_->policies().Equals(bundle));

  // OnUpdatePolicy is called even when there are no changes.
  MockConfigurationPolicyObserver observer;
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(provider_.get(), &observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(provider_->policies().Equals(bundle));

  // OnUpdatePolicy is called when there are changes.
  test_harness_->InstallStringPolicy(test_policy_definitions::kKeyString,
                                     "value");
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);

  bundle.Get(POLICY_DOMAIN_CHROME, "")
      .Set(test_policy_definitions::kKeyString,
           test_harness_->policy_level(),
           test_harness_->policy_scope(),
           base::Value::CreateStringValue("value"));
  EXPECT_TRUE(provider_->policies().Equals(bundle));
}

TEST(ConfigurationPolicyProviderTest, FixDeprecatedPolicies) {
  PolicyMap policy_map;
  policy_map.Set(key::kProxyServerMode,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 base::Value::CreateIntegerValue(3));

  // Both these policies should be ignored, since there's a higher priority
  // policy available.
  policy_map.Set(key::kProxyMode,
                 POLICY_LEVEL_RECOMMENDED,
                 POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("pac_script"));
  policy_map.Set(key::kProxyPacUrl,
                 POLICY_LEVEL_RECOMMENDED,
                 POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("http://example.com/wpad.dat"));

  MockConfigurationPolicyProvider provider;
  provider.UpdateChromePolicy(policy_map);

  PolicyBundle expected_bundle;
  base::DictionaryValue* expected_value = new base::DictionaryValue();
  expected_value->SetInteger(key::kProxyServerMode, 3);
  expected_bundle.Get(POLICY_DOMAIN_CHROME, "")
      .Set(key::kProxySettings, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           expected_value);
  EXPECT_TRUE(provider.policies().Equals(expected_bundle));
}

}  // namespace policy
