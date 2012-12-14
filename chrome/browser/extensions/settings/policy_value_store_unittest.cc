// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/policy_value_store.h"

#include "base/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/settings/settings_observer.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/value_store/leveldb_value_store.h"
#include "chrome/browser/value_store/value_store_unittest.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace extensions {

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

class MockSettingsObserver : public SettingsObserver {
 public:
  MOCK_METHOD3(OnSettingsChanged, void(
      const std::string& extension_id,
      settings_namespace::Namespace settings_namespace,
      const std::string& changes_json));
};

// Extends PolicyValueStore by overriding the mutating methods, so that the
// Get() base implementation can be tested with the ValueStoreTest parameterized
// tests.
class MutablePolicyValueStore : public PolicyValueStore {
 public:
  explicit MutablePolicyValueStore(const FilePath& path)
      : PolicyValueStore(kTestExtensionId,
                         make_scoped_refptr(new SettingsObserverList()),
                         scoped_ptr<ValueStore>(new LeveldbValueStore(path))) {}
  virtual ~MutablePolicyValueStore() {}

  virtual WriteResult Set(
      WriteOptions options,
      const std::string& key,
      const base::Value& value) OVERRIDE {
    return delegate()->Set(options, key, value);
  }

  virtual WriteResult Set(
      WriteOptions options, const base::DictionaryValue& values) OVERRIDE {
    return delegate()->Set(options, values);
  }

  virtual WriteResult Remove(const std::string& key) OVERRIDE {
    return delegate()->Remove(key);
  }

  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE {
    return delegate()->Remove(keys);
  }

  virtual WriteResult Clear() OVERRIDE {
    return delegate()->Clear();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MutablePolicyValueStore);
};

ValueStore* Param(const FilePath& file_path) {
  return new MutablePolicyValueStore(file_path);
}

}  // namespace

INSTANTIATE_TEST_CASE_P(
    PolicyValueStoreTest,
    ValueStoreTest,
    testing::Values(&Param));

class PolicyValueStoreTest : public testing::Test {
 public:
  PolicyValueStoreTest()
      : file_thread_(content::BrowserThread::FILE, &loop_) {}
  virtual ~PolicyValueStoreTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    observers_ = new SettingsObserverList();
    observers_->AddObserver(&observer_);
    store_.reset(new PolicyValueStore(
        kTestExtensionId,
        observers_,
        scoped_ptr<ValueStore>(
            new LeveldbValueStore(scoped_temp_dir_.path()))));
  }

  virtual void TearDown() OVERRIDE {
    observers_->RemoveObserver(&observer_);
    store_.reset();
  }

 protected:
  base::ScopedTempDir scoped_temp_dir_;
  MessageLoop loop_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<PolicyValueStore> store_;
  MockSettingsObserver observer_;
  scoped_refptr<SettingsObserverList> observers_;
};

TEST_F(PolicyValueStoreTest, DontProvideRecommendedPolicies) {
  policy::PolicyMap policies;
  base::FundamentalValue expected(123);
  policies.Set("must", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, expected.DeepCopy());
  policies.Set("may", policy::POLICY_LEVEL_RECOMMENDED,
               policy::POLICY_SCOPE_USER, base::Value::CreateIntegerValue(456));
  store_->SetCurrentPolicy(policies, false);
  ValueStore::ReadResult result = store_->Get();
  ASSERT_FALSE(result->HasError());
  EXPECT_EQ(1u, result->settings()->size());
  base::Value* value = NULL;
  EXPECT_FALSE(result->settings()->Get("may", &value));
  EXPECT_TRUE(result->settings()->Get("must", &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));
}

TEST_F(PolicyValueStoreTest, ReadOnly) {
  ValueStore::WriteOptions options = ValueStore::DEFAULTS;

  base::StringValue string_value("value");
  EXPECT_TRUE(store_->Set(options, "key", string_value)->HasError());

  base::DictionaryValue dict;
  dict.SetString("key", "value");
  EXPECT_TRUE(store_->Set(options, dict)->HasError());

  EXPECT_TRUE(store_->Remove("key")->HasError());
  std::vector<std::string> keys;
  keys.push_back("key");
  EXPECT_TRUE(store_->Remove(keys)->HasError());
  EXPECT_TRUE(store_->Clear()->HasError());
}

TEST_F(PolicyValueStoreTest, NotifyOnChanges) {
  policy::PolicyMap policies;
  policies.Set("aaa", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               base::Value::CreateStringValue("111"));
  EXPECT_CALL(observer_, OnSettingsChanged(_, _, _)).Times(0);
  // No notification when setting the initial policy.
  store_->SetCurrentPolicy(policies, false);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);

  // And no notifications on changes when not asked for.
  policies.Set("aaa", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               base::Value::CreateStringValue("222"));
  policies.Set("bbb", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               base::Value::CreateStringValue("223"));
  EXPECT_CALL(observer_, OnSettingsChanged(_, _, _)).Times(0);
  store_->SetCurrentPolicy(policies, false);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);

  // Notify when new policies are added.
  ValueStoreChangeList changes;
  base::StringValue value("333");
  changes.push_back(ValueStoreChange("ccc", NULL, value.DeepCopy()));
  policies.Set("ccc", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               value.DeepCopy());
  EXPECT_CALL(observer_, OnSettingsChanged(kTestExtensionId,
                                           settings_namespace::MANAGED,
                                           ValueStoreChange::ToJson(changes)));
  store_->SetCurrentPolicy(policies, true);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);

  // Notify when policies change.
  changes.clear();
  base::StringValue new_value("444");
  changes.push_back(
      ValueStoreChange("ccc", value.DeepCopy(), new_value.DeepCopy()));
  policies.Set("ccc", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               new_value.DeepCopy());
  EXPECT_CALL(observer_, OnSettingsChanged(kTestExtensionId,
                                           settings_namespace::MANAGED,
                                           ValueStoreChange::ToJson(changes)));
  store_->SetCurrentPolicy(policies, true);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);

  // Notify when policies are removed.
  changes.clear();
  changes.push_back(ValueStoreChange("ccc", new_value.DeepCopy(), NULL));
  policies.Erase("ccc");
  EXPECT_CALL(observer_, OnSettingsChanged(kTestExtensionId,
                                           settings_namespace::MANAGED,
                                           ValueStoreChange::ToJson(changes)));
  store_->SetCurrentPolicy(policies, true);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);

  // Don't notify when there aren't changes.
  EXPECT_CALL(observer_, OnSettingsChanged(_, _, _)).Times(0);
  store_->SetCurrentPolicy(policies, true);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace extensions
