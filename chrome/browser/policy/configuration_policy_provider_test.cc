// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_test.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
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

// In order to have correct enum values, we alias these to some actual policies.
const ConfigurationPolicyType kPolicyString = kPolicyHomepageLocation;
const ConfigurationPolicyType kPolicyBoolean = kPolicyHomepageIsNewTabPage;
const ConfigurationPolicyType kPolicyInteger = kPolicyRestoreOnStartup;
const ConfigurationPolicyType kPolicyStringList = kPolicyRestoreOnStartupURLs;

static const PolicyDefinitionList::Entry kEntries[] = {
  { kPolicyString, base::Value::TYPE_STRING, kKeyString },
  { kPolicyBoolean, base::Value::TYPE_BOOLEAN, kKeyBoolean },
  { kPolicyInteger, base::Value::TYPE_INTEGER, kKeyInteger },
  { kPolicyStringList, base::Value::TYPE_LIST, kKeyStringList },
};

const PolicyDefinitionList kList = {
  kEntries, kEntries + arraysize(kEntries)
};

}  // namespace test_policy_definitions

PolicyProviderTestHarness::PolicyProviderTestHarness() {}

PolicyProviderTestHarness::~PolicyProviderTestHarness() {}

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

  PolicyMap policy_map;
  EXPECT_TRUE(provider_->Provide(&policy_map));
  EXPECT_TRUE(policy_map.empty());
}

void ConfigurationPolicyProviderTest::TearDown() {
  // Give providers the chance to clean up after themselves on the file thread.
  provider_.reset();

  AsynchronousPolicyTestBase::TearDown();
}

void ConfigurationPolicyProviderTest::CheckValue(
    const char* policy_name,
    ConfigurationPolicyType policy_type,
    const base::Value& expected_value,
    base::Closure install_value) {
  // Install the value, reload policy and check the provider for the value.
  install_value.Run();
  provider_->RefreshPolicies();
  loop_.RunAllPending();
  PolicyMap policy_map;
  EXPECT_TRUE(provider_->Provide(&policy_map));
  EXPECT_EQ(1U, policy_map.size());
  EXPECT_TRUE(base::Value::Equals(&expected_value,
                                  policy_map.Get(policy_type)));
}

TEST_P(ConfigurationPolicyProviderTest, Empty) {
  provider_->RefreshPolicies();
  loop_.RunAllPending();
  PolicyMap policy_map;
  EXPECT_TRUE(provider_->Provide(&policy_map));
  EXPECT_TRUE(policy_map.empty());
}

TEST_P(ConfigurationPolicyProviderTest, StringValue) {
  const char kTestString[] = "string_value";
  StringValue expected_value(kTestString);
  CheckValue(test_policy_definitions::kKeyString,
             test_policy_definitions::kPolicyString,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallStringPolicy,
                        base::Unretained(test_harness_.get()),
                        test_policy_definitions::kKeyString,
                        kTestString));
}

TEST_P(ConfigurationPolicyProviderTest, BooleanValue) {
  base::FundamentalValue expected_value(true);
  CheckValue(test_policy_definitions::kKeyBoolean,
             test_policy_definitions::kPolicyBoolean,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallBooleanPolicy,
                        base::Unretained(test_harness_.get()),
                        test_policy_definitions::kKeyBoolean,
                        true));
}

TEST_P(ConfigurationPolicyProviderTest, IntegerValue) {
  base::FundamentalValue expected_value(42);
  CheckValue(test_policy_definitions::kKeyInteger,
             test_policy_definitions::kPolicyInteger,
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
             test_policy_definitions::kPolicyStringList,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallStringListPolicy,
                        base::Unretained(test_harness_.get()),
                        test_policy_definitions::kKeyStringList,
                        &expected_value));
}

TEST_P(ConfigurationPolicyProviderTest, RefreshPolicies) {
  PolicyMap policy_map;
  EXPECT_TRUE(provider_->Provide(&policy_map));
  EXPECT_EQ(0U, policy_map.size());

  // OnUpdatePolicy is called even when there are no changes.
  MockConfigurationPolicyObserver observer;
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(provider_.get(), &observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(provider_->Provide(&policy_map));
  EXPECT_EQ(0U, policy_map.size());

  // OnUpdatePolicy is called when there are changes.
  test_harness_->InstallStringPolicy(test_policy_definitions::kKeyString,
                                     "value");
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);

  policy_map.Clear();
  EXPECT_TRUE(provider_->Provide(&policy_map));
  EXPECT_EQ(1U, policy_map.size());
}

}  // namespace policy
