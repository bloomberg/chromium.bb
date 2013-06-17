// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

using WebKit::WebSecurityOrigin;
using content::IndexedDBBackingStore;
using content::LevelDBComparator;
using content::LevelDBDatabase;
using content::LevelDBFactory;
using content::LevelDBSlice;
using content::LevelDBSnapshot;

namespace {

class BustedLevelDBDatabase : public LevelDBDatabase {
 public:
  static scoped_ptr<LevelDBDatabase> Open(
      const base::FilePath& file_name,
      const LevelDBComparator* /*comparator*/) {
    return scoped_ptr<LevelDBDatabase>(new BustedLevelDBDatabase);
  }
  virtual bool Get(const LevelDBSlice& key,
                   std::string* value,
                   bool* found,
                   const LevelDBSnapshot* = 0) OVERRIDE {
    // false means IO error.
    return false;
  }
};

class MockLevelDBFactory : public LevelDBFactory {
 public:
  MockLevelDBFactory() : destroy_called_(false) {}
  virtual scoped_ptr<LevelDBDatabase> OpenLevelDB(
      const base::FilePath& file_name,
      const LevelDBComparator* comparator,
      bool* is_disk_full = 0) OVERRIDE {
    return BustedLevelDBDatabase::Open(file_name, comparator);
  }
  virtual bool DestroyLevelDB(const base::FilePath& file_name) OVERRIDE {
    EXPECT_FALSE(destroy_called_);
    destroy_called_ = true;
    return false;
  }
  virtual ~MockLevelDBFactory() { EXPECT_TRUE(destroy_called_); }

 private:
  bool destroy_called_;
};

TEST(IndexedDBIOErrorTest, CleanUpTest) {
  WebSecurityOrigin origin(
      WebSecurityOrigin::createFromString("http://localhost:81"));
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath path = temp_directory.path();
  string16 dummy_file_identifier;
  MockLevelDBFactory mock_leveldb_factory;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      IndexedDBBackingStore::Open(origin.databaseIdentifier(),
                                  path,
                                  dummy_file_identifier,
                                  &mock_leveldb_factory);
}

}  // namespace
