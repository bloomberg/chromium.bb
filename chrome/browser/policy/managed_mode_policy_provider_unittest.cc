// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/testing_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider_test.h"
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  virtual ~TestHarness();

  static PolicyProviderTestHarness* Create();

  virtual void SetUp() OVERRIDE;

  // PolicyProviderTestHarness implementation:
  virtual ConfigurationPolicyProvider* CreateProvider(
      const PolicyDefinitionList* policy_definition_list) OVERRIDE;

  virtual void InstallEmptyPolicy() OVERRIDE;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) OVERRIDE;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) OVERRIDE;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) OVERRIDE;
  virtual void InstallStringListPolicy(
      const std::string& policy_name,
      const base::ListValue* policy_value) OVERRIDE;
  virtual void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) OVERRIDE;

 private:
  void InstallPolicy(const std::string& policy_name, base::Value* policy_value);

  scoped_refptr<TestingPrefStore> pref_store_;

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness()
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER),
      pref_store_(new TestingPrefStore) {
  pref_store_->SetInitializationCompleted();
}

TestHarness::~TestHarness() {}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness();
}

void TestHarness::SetUp() {
}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    const PolicyDefinitionList* policy_definition_list) {
  return new ManagedModePolicyProvider(pref_store_);
}

void TestHarness::InstallEmptyPolicy() {}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  InstallPolicy(policy_name, base::Value::CreateStringValue(policy_value));
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  InstallPolicy(policy_name, base::Value::CreateIntegerValue(policy_value));
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  InstallPolicy(policy_name, base::Value::CreateBooleanValue(policy_value));
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const base::ListValue* policy_value) {
  InstallPolicy(policy_name, policy_value->DeepCopy());
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  InstallPolicy(policy_name, policy_value->DeepCopy());
}

void TestHarness::InstallPolicy(const std::string& policy_name,
                                base::Value* policy_value) {
  base::DictionaryValue* cached_policy = NULL;
  base::Value* value = NULL;
  if (pref_store_->GetMutableValue(ManagedModePolicyProvider::kPolicies,
                                   &value)) {
    ASSERT_TRUE(value->GetAsDictionary(&cached_policy));
  } else {
    cached_policy = new base::DictionaryValue;
    pref_store_->SetValue(ManagedModePolicyProvider::kPolicies,
                          cached_policy);
  }

  cached_policy->SetWithoutPathExpansion(policy_name, policy_value);
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_CASE_P(
    ManagedModePolicyProviderTest,
    ConfigurationPolicyProviderTest,
    testing::Values(TestHarness::Create));

class ManagedModePolicyProviderAPITest : public PolicyTestBase {
 protected:
  ManagedModePolicyProviderAPITest()
      : pref_store_(new TestingPrefStore),
        provider_(pref_store_) {
    pref_store_->SetInitializationCompleted();
  }
  virtual ~ManagedModePolicyProviderAPITest() {}

  virtual void SetUp() OVERRIDE {
    PolicyTestBase::SetUp();
    provider_.Init();
  }

  virtual void TearDown() OVERRIDE {
    provider_.Shutdown();
    PolicyTestBase::TearDown();
  }

  scoped_refptr<TestingPrefStore> pref_store_;
  ManagedModePolicyProvider provider_;
};

const char kPolicyKey[] = "TestingPolicy";

TEST_F(ManagedModePolicyProviderAPITest, Empty) {
  EXPECT_FALSE(provider_.GetPolicy(kPolicyKey));

  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_.policies().Equals(kEmptyBundle));
}

TEST_F(ManagedModePolicyProviderAPITest, SetPolicy) {
  base::StringValue policy_value("PolicyValue");
  provider_.SetPolicy(kPolicyKey, &policy_value);

  EXPECT_TRUE(base::Value::Equals(&policy_value,
                                  provider_.GetPolicy(kPolicyKey)));

  PolicyBundle expected_bundle;
  PolicyMap* policy_map = &expected_bundle.Get(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_map->Set(kPolicyKey, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  policy_value.DeepCopy());
  EXPECT_TRUE(provider_.policies().Equals(expected_bundle));

  // A newly-created provider should have the same policies.
  ManagedModePolicyProvider new_provider(pref_store_);
  new_provider.Init();
  EXPECT_TRUE(new_provider.policies().Equals(expected_bundle));

  // Cleanup.
  new_provider.Shutdown();
}

}  // namespace policy
