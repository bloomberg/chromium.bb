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

// A PrefHashStore that allows simulation of CheckValue results and captures
// checked and stored values.
class MockPrefHashStore : public PrefHashStore {
 public:
  static const char kNullPlaceholder[];

  MockPrefHashStore() {}

  // Set the result that will be returned when |path| is passed to |CheckValue|.
  void SetCheckResult(const std::string& path,
                      PrefHashStore::ValueState result);

  // Returns paths that have been passed to |CheckValue|.
  const std::set<std::string>& checked_paths() const {
    return checked_paths_;
  }

  // Returns paths that have been passed to |StoreHash|.
  const std::set<std::string>& stored_paths() const {
    return stored_paths_;
  }

  // Returns the value that was passed to |CheckValue| for |path|, rendered to a
  // string. If the checked value was NULL, the string is kNullPlaceholder.
  std::string checked_value(const std::string& path) const {
    std::map<std::string, std::string>::iterator value =
        checked_values_.find(path);
    if (value != checked_values_.end())
      return value->second;
    return std::string("No checked value.");
  }

  // Returns the value that was passed to |StoreHash| for |path|, rendered to a
  // string. If the stored value was NULL, the string is kNullPlaceholder.
  std::string stored_value(const std::string& path) const {
    std::map<std::string, std::string>::const_iterator value =
        stored_values_.find(path);
    if (value != stored_values_.end())
      return value->second;
    return std::string("No stored value.");
  }

  // PrefHashStore implementation.
  virtual PrefHashStore::ValueState CheckValue(
      const std::string& path, const base::Value* value) const OVERRIDE;
  virtual void StoreHash(const std::string& path,
                         const base::Value* new_value) OVERRIDE;

 private:
  std::map<std::string, PrefHashStore::ValueState> check_results_;
  mutable std::set<std::string> checked_paths_;
  std::set<std::string> stored_paths_;
  mutable std::map<std::string, std::string> checked_values_;
  std::map<std::string, std::string> stored_values_;

  DISALLOW_COPY_AND_ASSIGN(MockPrefHashStore);
};

const char MockPrefHashStore::kNullPlaceholder[] = "NULL";

void MockPrefHashStore::SetCheckResult(
    const std::string& path, PrefHashStore::ValueState result) {
  check_results_.insert(std::make_pair(path, result));
}

PrefHashStore::ValueState MockPrefHashStore::CheckValue(
    const std::string& path, const base::Value* value) const {
  EXPECT_TRUE(checked_paths_.insert(path).second);
  std::string as_string = kNullPlaceholder;
  if (value)
    EXPECT_TRUE(value->GetAsString(&as_string));

  checked_values_.insert(std::make_pair(path, as_string));
  std::map<std::string, PrefHashStore::ValueState>::const_iterator result =
      check_results_.find(path);
  if (result != check_results_.end())
    return result->second;
  return PrefHashStore::UNCHANGED;
}

void MockPrefHashStore::StoreHash(const std::string& path,
                                  const base::Value* new_value) {
  std::string as_string = kNullPlaceholder;
  if (new_value)
    EXPECT_TRUE(new_value->GetAsString(&as_string));
  stored_paths_.insert(path);
  stored_values_.insert(std::make_pair(path, as_string));
}

// Creates a PrefHashFilter that uses a MockPrefHashStore. The
// MockPrefHashStore (owned by the PrefHashFilter) is returned in
// |mock_pref_hash_store|.
scoped_ptr<PrefHashFilter> CreatePrefHashFilter(
    MockPrefHashStore** mock_pref_hash_store) {
  scoped_ptr<MockPrefHashStore> temp_mock_pref_hash_store(
      new MockPrefHashStore);
  if (mock_pref_hash_store)
    *mock_pref_hash_store = temp_mock_pref_hash_store.get();
  return scoped_ptr<PrefHashFilter>(
      new PrefHashFilter(temp_mock_pref_hash_store.PassAs<PrefHashStore>()));
}

class PrefHashFilterTest : public testing::Test {
 public:
  PrefHashFilterTest() : mock_pref_hash_store_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Capture the name of one tracked pref.
    MockPrefHashStore* temp_hash_store = NULL;
    scoped_ptr<PrefHashFilter> temp_filter =
        CreatePrefHashFilter(&temp_hash_store);
    temp_filter->FilterOnLoad(&pref_store_contents_);
    ASSERT_FALSE(temp_hash_store->checked_paths().empty());
    tracked_path_ = *temp_hash_store->checked_paths().begin();

    // Construct a PrefHashFilter and MockPrefHashStore for the test.
    pref_hash_filter_ = CreatePrefHashFilter(&mock_pref_hash_store_);
  }

 protected:
  std::string tracked_path_;
  MockPrefHashStore* mock_pref_hash_store_;
  base::DictionaryValue pref_store_contents_;
  scoped_ptr<PrefHashFilter> pref_hash_filter_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashFilterTest);
};

TEST_F(PrefHashFilterTest, EmptyAndUnchanged) {
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  // More than 0 paths checked.
  ASSERT_LT(0u, mock_pref_hash_store_->checked_paths().size());
  // No paths stored, since they all return |UNCHANGED|.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths().size());
  // Since there was nothing in |pref_store_contents_| the checked value should
  // have been NULL.
  ASSERT_EQ(MockPrefHashStore::kNullPlaceholder,
            mock_pref_hash_store_->checked_value(tracked_path_));
}

TEST_F(PrefHashFilterTest, FilterUpdate) {
  base::StringValue string_value("string value");
  pref_hash_filter_->FilterUpdate(tracked_path_, &string_value);
  // One path should be stored.
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths().size());
  ASSERT_EQ(tracked_path_, *mock_pref_hash_store_->stored_paths().begin());
  ASSERT_EQ("string value", mock_pref_hash_store_->stored_value(tracked_path_));
}

TEST_F(PrefHashFilterTest, EmptyAndUnknown){
  mock_pref_hash_store_->SetCheckResult(tracked_path_,
                                        PrefHashStore::UNKNOWN_VALUE);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_LT(0u, mock_pref_hash_store_->checked_paths().size());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths().size());
  ASSERT_EQ(tracked_path_, *mock_pref_hash_store_->stored_paths().begin());
  ASSERT_EQ(MockPrefHashStore::kNullPlaceholder,
            mock_pref_hash_store_->stored_value(tracked_path_));
}

TEST_F(PrefHashFilterTest, InitialValueUnknown) {
  pref_store_contents_.Set(tracked_path_,
                           new base::StringValue("string value"));

  mock_pref_hash_store_->SetCheckResult(tracked_path_,
                                        PrefHashStore::UNKNOWN_VALUE);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_LT(0u, mock_pref_hash_store_->checked_paths().size());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths().size());
  ASSERT_EQ(tracked_path_, *mock_pref_hash_store_->stored_paths().begin());
  ASSERT_EQ("string value", mock_pref_hash_store_->stored_value(tracked_path_));
}

TEST_F(PrefHashFilterTest, InitialValueChanged) {
  pref_store_contents_.Set(tracked_path_,
                           new base::StringValue("string value"));

  mock_pref_hash_store_->SetCheckResult(tracked_path_,
                                        PrefHashStore::CHANGED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_LT(0u, mock_pref_hash_store_->checked_paths().size());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths().size());
  ASSERT_EQ(tracked_path_, *mock_pref_hash_store_->stored_paths().begin());
  ASSERT_EQ("string value", mock_pref_hash_store_->stored_value(tracked_path_));
}

TEST_F(PrefHashFilterTest, EmptyCleared) {
  mock_pref_hash_store_->SetCheckResult(tracked_path_,
                                        PrefHashStore::CLEARED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_LT(0u, mock_pref_hash_store_->checked_paths().size());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths().size());
  ASSERT_EQ(tracked_path_, *mock_pref_hash_store_->stored_paths().begin());
  ASSERT_EQ(MockPrefHashStore::kNullPlaceholder,
            mock_pref_hash_store_->stored_value(tracked_path_));
}

TEST_F(PrefHashFilterTest, EmptyMigrated) {
  mock_pref_hash_store_->SetCheckResult(tracked_path_,
                                        PrefHashStore::MIGRATED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_LT(0u, mock_pref_hash_store_->checked_paths().size());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths().size());
  ASSERT_EQ(tracked_path_, *mock_pref_hash_store_->stored_paths().begin());
  ASSERT_EQ(MockPrefHashStore::kNullPlaceholder,
            mock_pref_hash_store_->stored_value(tracked_path_));
}
