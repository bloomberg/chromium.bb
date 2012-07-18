// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/value_store_unittest.h"

#include "chrome/browser/value_store/policy_value_store.h"

namespace extensions {

namespace {

// Extends PolicyValueStore by overriding the mutating methods, so that the
// Get() base implementation can be tested with the ValueStoreTest parameterized
// tests.
class MutablePolicyValueStore : public PolicyValueStore {
 public:
  MutablePolicyValueStore() : PolicyValueStore(&policy_map_) {}
  virtual ~MutablePolicyValueStore() {}

  virtual WriteResult Set(
      WriteOptions options,
      const std::string& key,
      const base::Value& value) OVERRIDE {
    ValueStoreChangeList* results = new ValueStoreChangeList();
    SetIfChanged(results, key, value);
    return MakeWriteResult(results);
  }

  virtual WriteResult Set(
      WriteOptions options, const base::DictionaryValue& values) OVERRIDE {
    ValueStoreChangeList* results = new ValueStoreChangeList();
    for (base::DictionaryValue::Iterator it(values);
         it.HasNext(); it.Advance()) {
      SetIfChanged(results, it.key(), it.value());
    }
    return MakeWriteResult(results);
  }

  virtual WriteResult Remove(const std::string& key) OVERRIDE {
    ValueStoreChangeList* results = new ValueStoreChangeList();
    RemoveIfExists(results, key);
    return MakeWriteResult(results);
  }

  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE {
    ValueStoreChangeList* results = new ValueStoreChangeList();
    for (std::vector<std::string>::const_iterator it = keys.begin();
         it != keys.end(); ++it) {
      RemoveIfExists(results, *it);
    }
    return MakeWriteResult(results);
  }

  virtual WriteResult Clear() OVERRIDE {
    ValueStoreChangeList* results = new ValueStoreChangeList();
    for (policy::PolicyMap::const_iterator it = policy_map_.begin();
         it != policy_map_.end(); ) {
      std::string key = it->first;
      ++it;
      RemoveIfExists(results, key);
    }
    return MakeWriteResult(results);
  }

 private:
  void SetIfChanged(ValueStoreChangeList* results,
                    const std::string& key,
                    const base::Value& value) {
    const base::Value* current_value = policy_map_.GetValue(key);
    if (base::Value::Equals(current_value, &value))
      return;
    base::Value* old_value = NULL;
    if (current_value)
      old_value = current_value->DeepCopy();
    results->push_back(ValueStoreChange(key, old_value, value.DeepCopy()));
    policy_map_.Set(key, policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, value.DeepCopy());
  }

  void RemoveIfExists(ValueStoreChangeList* results,
                      const std::string& key) {
    const base::Value* old_value = policy_map_.GetValue(key);
    if (old_value) {
      results->push_back(ValueStoreChange(key, old_value->DeepCopy(), NULL));
      policy_map_.Erase(key);
    }
  }

  policy::PolicyMap policy_map_;

  DISALLOW_COPY_AND_ASSIGN(MutablePolicyValueStore);
};

ValueStore* Param(const FilePath& file_path) {
  return new MutablePolicyValueStore();
}

}  // namespace

INSTANTIATE_TEST_CASE_P(
    PolicyValueStoreTest,
    ValueStoreTest,
    testing::Values(&Param));

TEST(PolicyValueStoreTest, DontProvideRecommendedPolicies) {
  policy::PolicyMap policies;
  base::FundamentalValue expected(123);
  policies.Set("must", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, expected.DeepCopy());
  policies.Set("may", policy::POLICY_LEVEL_RECOMMENDED,
               policy::POLICY_SCOPE_USER, base::Value::CreateIntegerValue(456));
  PolicyValueStore store(&policies);
  ValueStore::ReadResult result = store.Get();
  ASSERT_FALSE(result->HasError());
  EXPECT_EQ(1u, result->settings()->size());
  base::Value* value = NULL;
  EXPECT_FALSE(result->settings()->Get("may", &value));
  EXPECT_TRUE(result->settings()->Get("must", &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));
}

}  // namespace extensions
