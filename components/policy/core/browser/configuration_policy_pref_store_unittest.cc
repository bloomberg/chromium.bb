// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_pref_store.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_store_observer_mock.h"
#include "testing/gmock/include/gmock/gmock.h"

// Note: this file should move to components/policy/core/browser, but the
// components_unittests runner does not load the ResourceBundle as
// ChromeTestSuite::Initialize does, which leads to failures using
// PolicyErrorMap.

using testing::Mock;
using testing::Return;
using testing::_;

namespace {

const char kTestPolicy[] = "test.policy";
const char kTestPref[] = "test.pref";

}  // namespace

namespace policy {

// Test cases for list-valued policy settings.
class ConfigurationPolicyPrefStoreListTest
    : public ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    handler_list_.AddHandler(
        base::WrapUnique<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::Type::LIST)));
  }
};

TEST_F(ConfigurationPolicyPrefStoreListTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(kTestPref, nullptr));
}

TEST_F(ConfigurationPolicyPrefStoreListTest, SetValue) {
  std::unique_ptr<base::ListValue> in_value(new base::ListValue());
  in_value->AppendString("test1");
  in_value->AppendString("test2,");
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, in_value->CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(in_value->Equals(value));
}

// Test cases for string-valued policy settings.
class ConfigurationPolicyPrefStoreStringTest
    : public ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    handler_list_.AddHandler(
        base::WrapUnique<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::Type::STRING)));
  }
};

TEST_F(ConfigurationPolicyPrefStoreStringTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(kTestPref, nullptr));
}

TEST_F(ConfigurationPolicyPrefStoreStringTest, SetValue) {
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://chromium.org"), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(base::Value("http://chromium.org").Equals(value));
}

// Test cases for boolean-valued policy settings.
class ConfigurationPolicyPrefStoreBooleanTest
    : public ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    handler_list_.AddHandler(
        base::WrapUnique<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::Type::BOOLEAN)));
  }
};

TEST_F(ConfigurationPolicyPrefStoreBooleanTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(kTestPref, nullptr));
}

TEST_F(ConfigurationPolicyPrefStoreBooleanTest, SetValue) {
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  ASSERT_TRUE(value);
  bool boolean_value = true;
  bool result = value->GetAsBoolean(&boolean_value);
  ASSERT_TRUE(result);
  EXPECT_FALSE(boolean_value);

  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policy);
  value = nullptr;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  boolean_value = false;
  result = value->GetAsBoolean(&boolean_value);
  ASSERT_TRUE(result);
  EXPECT_TRUE(boolean_value);
}

// Test cases for integer-valued policy settings.
class ConfigurationPolicyPrefStoreIntegerTest
    : public ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    handler_list_.AddHandler(
        base::WrapUnique<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::Type::INTEGER)));
  }
};

TEST_F(ConfigurationPolicyPrefStoreIntegerTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(kTestPref, nullptr));
}

TEST_F(ConfigurationPolicyPrefStoreIntegerTest, SetValue) {
  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(2), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value(2).Equals(value));
}

// Exercises the policy refresh mechanism.
class ConfigurationPolicyPrefStoreRefreshTest
    : public ConfigurationPolicyPrefStoreTest {
 protected:
  void SetUp() override {
    ConfigurationPolicyPrefStoreTest::SetUp();
    store_->AddObserver(&observer_);
    handler_list_.AddHandler(
        base::WrapUnique<ConfigurationPolicyHandler>(new SimplePolicyHandler(
            kTestPolicy, kTestPref, base::Value::Type::STRING)));
  }

  void TearDown() override {
    store_->RemoveObserver(&observer_);
    ConfigurationPolicyPrefStoreTest::TearDown();
  }

  PrefStoreObserverMock observer_;
};

TEST_F(ConfigurationPolicyPrefStoreRefreshTest, Refresh) {
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(kTestPolicy, nullptr));

  PolicyMap policy;
  policy.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://www.chromium.org"), nullptr);
  UpdateProviderPolicy(policy);
  observer_.VerifyAndResetChangedKey(kTestPref);
  EXPECT_TRUE(store_->GetValue(kTestPref, &value));
  EXPECT_TRUE(base::Value("http://www.chromium.org").Equals(value));

  UpdateProviderPolicy(policy);
  EXPECT_TRUE(observer_.changed_keys.empty());

  policy.Erase(kTestPolicy);
  UpdateProviderPolicy(policy);
  observer_.VerifyAndResetChangedKey(kTestPref);
  EXPECT_FALSE(store_->GetValue(kTestPref, nullptr));
}

TEST_F(ConfigurationPolicyPrefStoreRefreshTest, Initialization) {
  EXPECT_FALSE(store_->IsInitializationComplete());
  EXPECT_CALL(provider_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  PolicyMap policy;
  UpdateProviderPolicy(policy);
  EXPECT_TRUE(observer_.initialized);
  EXPECT_TRUE(observer_.initialization_success);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(store_->IsInitializationComplete());
}

}  // namespace policy
