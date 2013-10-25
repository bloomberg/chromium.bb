// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_pref_store_unittest.h"

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_store_observer_mock.h"
#include "base/run_loop.h"
#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Mock;
using testing::Return;
using testing::_;

namespace {
const char kTestPolicy[] = "test.policy";
const char kTestPref[] = "test.pref";
}  // namespace

namespace policy {

// Holds a set of test parameters, consisting of pref name and policy name.
class PolicyAndPref {
 public:
  PolicyAndPref(const char* policy_name, const char* pref_name)
      : policy_name_(policy_name),
        pref_name_(pref_name) {}

  const char* policy_name() const { return policy_name_; }
  const char* pref_name() const { return pref_name_; }

 private:
  const char* policy_name_;
  const char* pref_name_;
};

ConfigurationPolicyPrefStoreTest::ConfigurationPolicyPrefStoreTest() {
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(false));
  provider_.Init();
  PolicyServiceImpl::Providers providers;
  providers.push_back(&provider_);
  policy_service_.reset(new PolicyServiceImpl(providers));
  store_ = new ConfigurationPolicyPrefStore(
      policy_service_.get(), &handler_list_, POLICY_LEVEL_MANDATORY);
}

ConfigurationPolicyPrefStoreTest::~ConfigurationPolicyPrefStoreTest() {}

void ConfigurationPolicyPrefStoreTest::TearDown() {
  provider_.Shutdown();
}

void ConfigurationPolicyPrefStoreTest::UpdateProviderPolicy(
    const PolicyMap& policy) {
  provider_.UpdateChromePolicy(policy);
  base::RunLoop loop;
  loop.RunUntilIdle();
}

// Test cases for list-valued policy settings.
class ConfigurationPolicyPrefStoreListTest
    : public ConfigurationPolicyPrefStoreTest {
  virtual void SetUp() OVERRIDE {
    handler_list_.AddHandler(
        make_scoped_ptr<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::TYPE_LIST)));
  }
};

TEST_F(ConfigurationPolicyPrefStoreListTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(kTestPref, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreListTest, SetValue) {
  base::ListValue* in_value = new base::ListValue();
  in_value->Append(base::Value::CreateStringValue("test1"));
  in_value->Append(base::Value::CreateStringValue("test2,"));
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, in_value, NULL);
  UpdateProviderPolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(in_value->Equals(value));
}

// Test cases for string-valued policy settings.
class ConfigurationPolicyPrefStoreStringTest
    : public ConfigurationPolicyPrefStoreTest {
  virtual void SetUp() OVERRIDE {
    handler_list_.AddHandler(
        make_scoped_ptr<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::TYPE_STRING)));
  }
};

TEST_F(ConfigurationPolicyPrefStoreStringTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(kTestPref, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreStringTest, SetValue) {
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://chromium.org"), NULL);
  UpdateProviderPolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(base::StringValue("http://chromium.org").Equals(value));
}

// Test cases for boolean-valued policy settings.
class ConfigurationPolicyPrefStoreBooleanTest
    : public ConfigurationPolicyPrefStoreTest {
  virtual void SetUp() OVERRIDE {
    handler_list_.AddHandler(
        make_scoped_ptr<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::TYPE_BOOLEAN)));
  }
};

TEST_F(ConfigurationPolicyPrefStoreBooleanTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(kTestPref, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreBooleanTest, SetValue) {
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false), NULL);
  UpdateProviderPolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  ASSERT_TRUE(value);
  bool boolean_value = true;
  bool result = value->GetAsBoolean(&boolean_value);
  ASSERT_TRUE(result);
  EXPECT_FALSE(boolean_value);

  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true), NULL);
  UpdateProviderPolicy(policy);
  value = NULL;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  boolean_value = false;
  result = value->GetAsBoolean(&boolean_value);
  ASSERT_TRUE(result);
  EXPECT_TRUE(boolean_value);
}

// Test cases for integer-valued policy settings.
class ConfigurationPolicyPrefStoreIntegerTest
    : public ConfigurationPolicyPrefStoreTest {
  virtual void SetUp() OVERRIDE {
    handler_list_.AddHandler(
        make_scoped_ptr<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::TYPE_INTEGER)));
  }
};

TEST_F(ConfigurationPolicyPrefStoreIntegerTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(kTestPref, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreIntegerTest, SetValue) {
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateIntegerValue(2), NULL);
  UpdateProviderPolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  EXPECT_TRUE(base::FundamentalValue(2).Equals(value));
}

// Exercises the policy refresh mechanism.
class ConfigurationPolicyPrefStoreRefreshTest
    : public ConfigurationPolicyPrefStoreTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ConfigurationPolicyPrefStoreTest::SetUp();
    store_->AddObserver(&observer_);
    handler_list_.AddHandler(
        make_scoped_ptr<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::TYPE_STRING)));
  }

  virtual void TearDown() OVERRIDE {
    store_->RemoveObserver(&observer_);
    ConfigurationPolicyPrefStoreTest::TearDown();
  }

  PrefStoreObserverMock observer_;
};

TEST_F(ConfigurationPolicyPrefStoreRefreshTest, Refresh) {
  const base::Value* value = NULL;
  EXPECT_FALSE(store_->GetValue(kTestPolicy, NULL));

  EXPECT_CALL(observer_, OnPrefValueChanged(kTestPref)).Times(1);
  PolicyMap policy;
  policy.Set(kTestPolicy,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://www.chromium.org"),
             NULL);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  EXPECT_TRUE(base::StringValue("http://www.chromium.org").Equals(value));

  EXPECT_CALL(observer_, OnPrefValueChanged(_)).Times(0);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnPrefValueChanged(kTestPref)).Times(1);
  policy.Erase(kTestPolicy);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(store_->GetValue(kTestPref, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreRefreshTest, Initialization) {
  EXPECT_FALSE(store_->IsInitializationComplete());
  EXPECT_CALL(provider_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(observer_, OnInitializationCompleted(true)).Times(1);
  PolicyMap policy;
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(store_->IsInitializationComplete());
}

}  // namespace policy
