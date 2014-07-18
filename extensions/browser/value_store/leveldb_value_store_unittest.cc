// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store_unittest.h"

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/value_store/leveldb_value_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {

ValueStore* Param(const base::FilePath& file_path) {
  return new LeveldbValueStore(file_path);
}

}  // namespace

INSTANTIATE_TEST_CASE_P(
    LeveldbValueStore,
    ValueStoreTest,
    testing::Values(&Param));

class LeveldbValueStoreUnitTest : public testing::Test {
 public:
  LeveldbValueStoreUnitTest() {}
  virtual ~LeveldbValueStoreUnitTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    OpenStore();
    ASSERT_FALSE(store_->Get()->HasError());
  }

  virtual void TearDown() OVERRIDE {
    store_->Clear();
    store_.reset();
  }

  void CloseStore() { store_.reset(); }

  void OpenStore() { store_.reset(new LeveldbValueStore(database_path())); }

  LeveldbValueStore* store() { return store_.get(); }
  const base::FilePath& database_path() { return database_dir_.path(); }

 private:
  scoped_ptr<LeveldbValueStore> store_;
  base::ScopedTempDir database_dir_;

  content::TestBrowserThreadBundle thread_bundle_;
};

// Check that we can restore a single corrupted key in the LeveldbValueStore.
TEST_F(LeveldbValueStoreUnitTest, RestoreKeyTest) {
  const char kNotCorruptKey[] = "not-corrupt";
  const char kValue[] = "value";

  // Insert a valid pair.
  scoped_ptr<base::Value> value(new base::StringValue(kValue));
  ASSERT_FALSE(
      store()->Set(ValueStore::DEFAULTS, kNotCorruptKey, *value)->HasError());

  // Insert a corrupt pair.
  const char kCorruptKey[] = "corrupt";
  leveldb::WriteBatch batch;
  batch.Put(kCorruptKey, "[{(.*+\"\'\\");
  ASSERT_TRUE(store()->WriteToDbForTest(&batch));

  // Verify corruption.
  ValueStore::ReadResult result = store()->Get(kCorruptKey);
  ASSERT_TRUE(result->HasError());
  ASSERT_EQ(ValueStore::CORRUPTION, result->error().code);

  // Restore and verify.
  ASSERT_TRUE(store()->RestoreKey(kCorruptKey));
  result = store()->Get(kCorruptKey);
  EXPECT_FALSE(result->HasError());
  EXPECT_TRUE(result->settings().empty());

  // Verify that the valid pair is still present.
  result = store()->Get(kNotCorruptKey);
  EXPECT_FALSE(result->HasError());
  EXPECT_TRUE(result->settings().HasKey(kNotCorruptKey));
  std::string value_string;
  EXPECT_TRUE(result->settings().GetString(kNotCorruptKey, &value_string));
  EXPECT_EQ(kValue, value_string);
}

// Test that the Restore() method does not just delete the entire database
// (unless absolutely necessary), and instead only removes corrupted keys.
TEST_F(LeveldbValueStoreUnitTest, RestoreDoesMinimumNecessary) {
  const char* kNotCorruptKeys[] = {"a", "n", "z"};
  const size_t kNotCorruptKeysSize = 3u;
  const char kCorruptKey1[] = "f";
  const char kCorruptKey2[] = "s";
  const char kValue[] = "value";
  const char kCorruptValue[] = "[{(.*+\"\'\\";

  // Insert a collection of non-corrupted pairs.
  scoped_ptr<base::Value> value(new base::StringValue(kValue));
  for (size_t i = 0; i < kNotCorruptKeysSize; ++i) {
    ASSERT_FALSE(store()
                     ->Set(ValueStore::DEFAULTS, kNotCorruptKeys[i], *value)
                     ->HasError());
  }

  // Insert a few corrupted pairs.
  leveldb::WriteBatch batch;
  batch.Put(kCorruptKey1, kCorruptValue);
  batch.Put(kCorruptKey2, kCorruptValue);
  ASSERT_TRUE(store()->WriteToDbForTest(&batch));

  // Verify that we broke it, and then fix it.
  ValueStore::ReadResult result = store()->Get();
  ASSERT_TRUE(result->HasError());
  ASSERT_EQ(ValueStore::CORRUPTION, result->error().code);

  ASSERT_TRUE(store()->Restore());

  // We should still have all valid pairs present in the database.
  std::string value_string;
  for (size_t i = 0; i < kNotCorruptKeysSize; ++i) {
    result = store()->Get(kNotCorruptKeys[i]);
    EXPECT_FALSE(result->HasError());
    EXPECT_TRUE(result->settings().HasKey(kNotCorruptKeys[i]));
    EXPECT_TRUE(
        result->settings().GetString(kNotCorruptKeys[i], &value_string));
    EXPECT_EQ(kValue, value_string);
  }
}

// Test that the LeveldbValueStore can recover in the case of a CATastrophic
// failure and we have total corruption. In this case, the database is plagued
// by LolCats.
// Full corruption has been known to happen occasionally in strange edge cases,
// such as after users use Windows Restore. We can't prevent it, but we need to
// be able to handle it smoothly.
TEST_F(LeveldbValueStoreUnitTest, RestoreFullDatabase) {
  const std::string kLolCats("I can haz leveldb filez?");
  const char* kNotCorruptKeys[] = {"a", "n", "z"};
  const size_t kNotCorruptKeysSize = 3u;
  const char kValue[] = "value";

  // Generate a database.
  scoped_ptr<base::Value> value(new base::StringValue(kValue));
  for (size_t i = 0; i < kNotCorruptKeysSize; ++i) {
    ASSERT_FALSE(store()
                     ->Set(ValueStore::DEFAULTS, kNotCorruptKeys[i], *value)
                     ->HasError());
  }

  // Close it (so we remove the lock), and replace all files with LolCats.
  CloseStore();
  base::FileEnumerator enumerator(
      database_path(), true /* recursive */, base::FileEnumerator::FILES);
  for (base::FilePath file = enumerator.Next(); !file.empty();
       file = enumerator.Next()) {
    // WriteFile() failure is a result of -1.
    ASSERT_NE(base::WriteFile(file, kLolCats.c_str(), kLolCats.length()),
              -1);
  }
  OpenStore();

  // We should definitely have an error.
  ValueStore::ReadResult result = store()->Get();
  ASSERT_TRUE(result->HasError());
  ASSERT_EQ(ValueStore::CORRUPTION, result->error().code);

  ASSERT_TRUE(store()->Restore());
  result = store()->Get();
  EXPECT_FALSE(result->HasError());
  // We couldn't recover anything, but we should be in a sane state again.
  EXPECT_EQ(0u, result->settings().size());
}
