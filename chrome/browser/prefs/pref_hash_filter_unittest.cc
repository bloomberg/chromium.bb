// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_filter.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestPref[] = "pref";
const char kTestPref2[] = "pref2";
const char kTestPref3[] = "pref3";
const char kReportOnlyPref[] = "report_only";

const PrefHashFilter::TrackedPreference kTestTrackedPrefs[] = {
  { 0, kTestPref, true },
  { 1, kReportOnlyPref, false },
  { 2, kTestPref2, true },
  { 3, kTestPref3, true },
};

}  // namespace

// A PrefHashStore that allows simulation of CheckValue results and captures
// checked and stored values.
class MockPrefHashStore : public PrefHashStore {
 public:

  MockPrefHashStore() {}

  // Set the result that will be returned when |path| is passed to |CheckValue|.
  void SetCheckResult(const std::string& path,
                      PrefHashStore::ValueState result);

  // Returns the number of paths checked.
  size_t checked_paths_count() const {
    return checked_values_.size();
  }

  // Returns the number of paths stored.
  size_t stored_paths_count() const {
    return stored_values_.size();
  }

  // Returns the pointer value that was passed to |CheckValue| for |path|. The
  // returned pointer could since have been freed and is thus not safe to
  // dereference.
  const void* checked_value(const std::string& path) const {
    std::map<std::string, const base::Value*>::iterator value =
        checked_values_.find(path);
    if (value != checked_values_.end())
      return value->second;
    return reinterpret_cast<void*>(0xBAD);
  }

  // Returns the pointer value that was passed to |StoreHash| for |path|. The
  // returned pointer could since have been freed and is thus not safe to
  // dereference.
  const void* stored_value(const std::string& path) const {
    std::map<std::string, const base::Value*>::const_iterator value =
        stored_values_.find(path);
    if (value != stored_values_.end())
      return value->second;
    return reinterpret_cast<void*>(0xBAD);
  }

  // PrefHashStore implementation.
  virtual PrefHashStore::ValueState CheckValue(
      const std::string& path, const base::Value* value) const OVERRIDE;
  virtual void StoreHash(const std::string& path,
                         const base::Value* new_value) OVERRIDE;

 private:
  std::map<std::string, PrefHashStore::ValueState> check_results_;
  mutable std::map<std::string, const base::Value*> checked_values_;
  std::map<std::string, const base::Value*> stored_values_;

  DISALLOW_COPY_AND_ASSIGN(MockPrefHashStore);
};

void MockPrefHashStore::SetCheckResult(
    const std::string& path, PrefHashStore::ValueState result) {
  check_results_.insert(std::make_pair(path, result));
}

PrefHashStore::ValueState MockPrefHashStore::CheckValue(
    const std::string& path, const base::Value* value) const {
  checked_values_.insert(std::make_pair(path, value));

  std::map<std::string, PrefHashStore::ValueState>::const_iterator result =
      check_results_.find(path);
  if (result != check_results_.end())
    return result->second;
  return PrefHashStore::UNCHANGED;
}

void MockPrefHashStore::StoreHash(const std::string& path,
                                  const base::Value* new_value) {
  stored_values_.insert(std::make_pair(path, new_value));
}

// Creates a PrefHashFilter that uses a MockPrefHashStore. The
// MockPrefHashStore (owned by the PrefHashFilter) is returned in
// |mock_pref_hash_store|.
scoped_ptr<PrefHashFilter> CreatePrefHashFilter(
    PrefHashFilter::EnforcementLevel enforcement_level,
    MockPrefHashStore** mock_pref_hash_store) {
  scoped_ptr<MockPrefHashStore> temp_mock_pref_hash_store(
      new MockPrefHashStore);
  if (mock_pref_hash_store)
    *mock_pref_hash_store = temp_mock_pref_hash_store.get();
  return scoped_ptr<PrefHashFilter>(
      new PrefHashFilter(temp_mock_pref_hash_store.PassAs<PrefHashStore>(),
                         kTestTrackedPrefs, arraysize(kTestTrackedPrefs),
                         arraysize(kTestTrackedPrefs),
                         enforcement_level));
}

class PrefHashFilterTest
    : public testing::TestWithParam<PrefHashFilter::EnforcementLevel> {
 public:
  PrefHashFilterTest() : mock_pref_hash_store_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Construct a PrefHashFilter and MockPrefHashStore for the test.
    pref_hash_filter_ = CreatePrefHashFilter(GetParam(),
                                             &mock_pref_hash_store_);
  }

 protected:
  MockPrefHashStore* mock_pref_hash_store_;
  base::DictionaryValue pref_store_contents_;
  scoped_ptr<PrefHashFilter> pref_hash_filter_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashFilterTest);
};

TEST_P(PrefHashFilterTest, EmptyAndUnchanged) {
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  // All paths checked.
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  // No paths stored, since they all return |UNCHANGED|.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
  // Since there was nothing in |pref_store_contents_| the checked value should
  // have been NULL for all tracked preferences.
  for (size_t i = 0; i < arraysize(kTestTrackedPrefs); ++i) {
    ASSERT_EQ(NULL,
              mock_pref_hash_store_->checked_value(kTestTrackedPrefs[i].name));
  }
}

TEST_P(PrefHashFilterTest, FilterTrackedPrefUpdate) {
  base::DictionaryValue root_dict;
  // Ownership of |string_value| is transfered to |root_dict|.
  base::Value* string_value = base::Value::CreateStringValue("string value");
  root_dict.Set(kTestPref, string_value);

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kTestPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData.
  pref_hash_filter_->FilterSerializeData(&root_dict);
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(string_value, mock_pref_hash_store_->stored_value(kTestPref));
}

TEST_P(PrefHashFilterTest, FilterUntrackedPrefUpdate) {
  base::DictionaryValue root_dict;
  root_dict.Set("untracked", base::Value::CreateStringValue("some value"));
  pref_hash_filter_->FilterUpdate("untracked");

  // No paths should be stored on FilterUpdate.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // Nor on FilterSerializeData.
  pref_hash_filter_->FilterSerializeData(&root_dict);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
}

TEST_P(PrefHashFilterTest, MultiplePrefsFilterSerializeData) {
  base::DictionaryValue root_dict;
  // Ownership of the following values is transfered to |root_dict|.
  base::Value* int_value1 = base::Value::CreateIntegerValue(1);
  base::Value* int_value2 = base::Value::CreateIntegerValue(2);
  base::Value* int_value3 = base::Value::CreateIntegerValue(3);
  base::Value* int_value4 = base::Value::CreateIntegerValue(4);
  root_dict.Set(kTestPref, int_value1);
  root_dict.Set(kTestPref2, int_value2);
  root_dict.Set(kTestPref3, int_value3);
  root_dict.Set("untracked", int_value4);

  // Only update kTestPref and kTestPref3
  pref_hash_filter_->FilterUpdate(kTestPref);
  pref_hash_filter_->FilterUpdate(kTestPref3);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // Update kTestPref3 again, nothing should be stored still.
  base::Value* int_value5 = base::Value::CreateIntegerValue(5);
  root_dict.Set(kTestPref3, int_value5);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // On FilterSerializeData, only kTestPref and kTestPref3 should get a new
  // hash.
  pref_hash_filter_->FilterSerializeData(&root_dict);
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(int_value1, mock_pref_hash_store_->stored_value(kTestPref));
  ASSERT_EQ(int_value5, mock_pref_hash_store_->stored_value(kTestPref3));
}

TEST_P(PrefHashFilterTest, EmptyAndUnknown) {
  ASSERT_FALSE(pref_store_contents_.Get(kTestPref, NULL));
  // NULL values are always trusted by the PrefHashStore.
  mock_pref_hash_store_->SetCheckResult(kTestPref,
                                        PrefHashStore::TRUSTED_UNKNOWN_VALUE);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());

  ASSERT_FALSE(pref_store_contents_.Get(kTestPref, NULL));
  ASSERT_EQ(NULL, mock_pref_hash_store_->stored_value(kTestPref));
}

TEST_P(PrefHashFilterTest, InitialValueUnknown) {
  // Ownership of this value is transfered to |pref_store_contents_|.
  base::Value* string_value = base::Value::CreateStringValue("test");
  pref_store_contents_.Set(kTestPref, string_value);

  ASSERT_TRUE(pref_store_contents_.Get(kTestPref, NULL));

  mock_pref_hash_store_->SetCheckResult(kTestPref,
                                        PrefHashStore::UNTRUSTED_UNKNOWN_VALUE);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());

  if (GetParam() >= PrefHashFilter::ENFORCE_NO_SEEDING) {
    // Ensure the pref was cleared and the hash for NULL was restored if the
    // current enforcement level denies seeding.
    ASSERT_FALSE(pref_store_contents_.Get(kTestPref, NULL));
    ASSERT_EQ(NULL, mock_pref_hash_store_->stored_value(kTestPref));
  } else {
    // Otherwise the value should have remained intact and the hash should have
    // been updated to match it.
    const base::Value* value_in_store;
    ASSERT_TRUE(pref_store_contents_.Get(kTestPref, &value_in_store));
    ASSERT_EQ(string_value, value_in_store);
    ASSERT_EQ(string_value, mock_pref_hash_store_->stored_value(kTestPref));
  }
}

TEST_P(PrefHashFilterTest, InitialValueTrustedUnknown) {
  // Ownership of this value is transfered to |pref_store_contents_|.
  base::Value* string_value = base::Value::CreateStringValue("test");
  pref_store_contents_.Set(kTestPref, string_value);

  ASSERT_TRUE(pref_store_contents_.Get(kTestPref, NULL));

  mock_pref_hash_store_->SetCheckResult(kTestPref,
                                        PrefHashStore::TRUSTED_UNKNOWN_VALUE);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());

  // Seeding is always allowed for trusted unknown values.
  const base::Value* value_in_store;
  ASSERT_TRUE(pref_store_contents_.Get(kTestPref, &value_in_store));
  ASSERT_EQ(string_value, value_in_store);
  ASSERT_EQ(string_value, mock_pref_hash_store_->stored_value(kTestPref));
}

TEST_P(PrefHashFilterTest, InitialValueChanged) {
  // Ownership of this value is transfered to |pref_store_contents_|.
  base::Value* int_value = base::Value::CreateIntegerValue(1234);
  pref_store_contents_.Set(kTestPref, int_value);

  ASSERT_TRUE(pref_store_contents_.Get(kTestPref, NULL));

  mock_pref_hash_store_->SetCheckResult(kTestPref, PrefHashStore::CHANGED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());

  if (GetParam() >= PrefHashFilter::ENFORCE) {
    // Ensure the pref was cleared and the hash for NULL was restored if the
    // current enforcement level prevents changes.
    ASSERT_FALSE(pref_store_contents_.Get(kTestPref, NULL));
    ASSERT_EQ(NULL, mock_pref_hash_store_->stored_value(kTestPref));
  } else {
    // Otherwise the value should have remained intact and the hash should have
    // been updated to match it.
    const base::Value* value_in_store;
    ASSERT_TRUE(pref_store_contents_.Get(kTestPref, &value_in_store));
    ASSERT_EQ(int_value, value_in_store);
    ASSERT_EQ(int_value, mock_pref_hash_store_->stored_value(kTestPref));
  }
}

TEST_P(PrefHashFilterTest, EmptyCleared) {
  ASSERT_FALSE(pref_store_contents_.Get(kTestPref, NULL));
  mock_pref_hash_store_->SetCheckResult(kTestPref, PrefHashStore::CLEARED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());

  // Regardless of the enforcement level, the only thing that should be done is
  // to restore the hash for NULL. The value itself should still be NULL.
  ASSERT_FALSE(pref_store_contents_.Get(kTestPref, NULL));
  ASSERT_EQ(NULL, mock_pref_hash_store_->stored_value(kTestPref));
}

TEST_P(PrefHashFilterTest, EmptyMigrated) {
  // Ownership of this value is transfered to |pref_store_contents_|.
  base::ListValue* list_value = new base::ListValue;
  list_value->Append(base::Value::CreateStringValue("test"));
  pref_store_contents_.Set(kTestPref, list_value);

  ASSERT_TRUE(pref_store_contents_.Get(kTestPref, NULL));

  mock_pref_hash_store_->SetCheckResult(kTestPref, PrefHashStore::MIGRATED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());

  if (GetParam() >= PrefHashFilter::ENFORCE_NO_SEEDING_NO_MIGRATION) {
    // Ensure the pref was cleared and the hash for NULL was restored if the
    // current enforcement level prevents migration.
    ASSERT_FALSE(pref_store_contents_.Get(kTestPref, NULL));
    ASSERT_EQ(NULL, mock_pref_hash_store_->stored_value(kTestPref));
  } else {
    // Otherwise the value should have remained intact and the hash should have
    // been updated to match it.
    const base::Value* value_in_store;
    ASSERT_TRUE(pref_store_contents_.Get(kTestPref, &value_in_store));
    ASSERT_EQ(list_value, value_in_store);
    ASSERT_EQ(list_value, mock_pref_hash_store_->stored_value(kTestPref));
  }
}

TEST_P(PrefHashFilterTest, DontResetReportOnly) {
  // Ownership of these values is transfered to |pref_store_contents_|.
  base::Value* int_value1 = base::Value::CreateIntegerValue(1);
  base::Value* int_value2 = base::Value::CreateIntegerValue(2);
  base::Value* report_only_val = base::Value::CreateIntegerValue(3);
  pref_store_contents_.Set(kTestPref, int_value1);
  pref_store_contents_.Set(kTestPref2, int_value2);
  pref_store_contents_.Set(kReportOnlyPref, report_only_val);

  ASSERT_TRUE(pref_store_contents_.Get(kTestPref, NULL));
  ASSERT_TRUE(pref_store_contents_.Get(kTestPref2, NULL));
  ASSERT_TRUE(pref_store_contents_.Get(kReportOnlyPref, NULL));

  mock_pref_hash_store_->SetCheckResult(kTestPref, PrefHashStore::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kTestPref2, PrefHashStore::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kReportOnlyPref,
                                        PrefHashStore::CHANGED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  // All prefs should be checked and a new hash should be stored for each tested
  // pref.
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(3u, mock_pref_hash_store_->stored_paths_count());

  // No matter what the enforcement level is, the report only pref should never
  // be reset.
  ASSERT_TRUE(pref_store_contents_.Get(kReportOnlyPref, NULL));
  ASSERT_EQ(report_only_val,
            mock_pref_hash_store_->stored_value(kReportOnlyPref));

  // All other prefs should have been reset if the enforcement level allows it.
  if (GetParam() >= PrefHashFilter::ENFORCE) {
    ASSERT_FALSE(pref_store_contents_.Get(kTestPref, NULL));
    ASSERT_FALSE(pref_store_contents_.Get(kTestPref2, NULL));
    ASSERT_EQ(NULL, mock_pref_hash_store_->stored_value(kTestPref));
    ASSERT_EQ(NULL, mock_pref_hash_store_->stored_value(kTestPref2));
  } else {
    const base::Value* value_in_store;
    const base::Value* value_in_store2;
    ASSERT_TRUE(pref_store_contents_.Get(kTestPref, &value_in_store));
    ASSERT_TRUE(pref_store_contents_.Get(kTestPref2, &value_in_store2));
    ASSERT_EQ(int_value1, value_in_store);
    ASSERT_EQ(int_value1, mock_pref_hash_store_->stored_value(kTestPref));
    ASSERT_EQ(int_value2, value_in_store2);
    ASSERT_EQ(int_value2, mock_pref_hash_store_->stored_value(kTestPref2));
  }
}

// This is a hack to allow testing::Range to iterate over enum values in
// PrefHashFilter::EnforcementLevel. This is required as
// testing::internals::RangeGenerator used by testing::Range needs to be able
// to do |i = i + step| where i is an EnforcementLevel and |step| is 1 by
// default; |enum + 1| results in an |int| type, not an |enum|, and there is no
// implicit conversion from |int| to |enum|. This hack works around this
// limitation by making |step| an |EnforcementLevelIncrement| which forces the
// explicit cast in the overloaded operator+ below and makes |i = i + step| a
// valid statement.
class EnforcementLevelIncrement {};
PrefHashFilter::EnforcementLevel operator+(
    PrefHashFilter::EnforcementLevel current_level,
    const EnforcementLevelIncrement& /* increment */) {
  return static_cast<PrefHashFilter::EnforcementLevel>(current_level + 1);
}

INSTANTIATE_TEST_CASE_P(
    PrefEnforcementLevels, PrefHashFilterTest,
    testing::Range(PrefHashFilter::NO_ENFORCEMENT,
                   static_cast<PrefHashFilter::EnforcementLevel>(
                       PrefHashFilter::ENFORCE_ALL + 1),
                   EnforcementLevelIncrement()));
