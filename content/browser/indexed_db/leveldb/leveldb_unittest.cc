// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstring>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/platform_file.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/env_idb.h"

namespace content {

namespace {

class SimpleComparator : public LevelDBComparator {
 public:
  virtual int Compare(const base::StringPiece& a,
                      const base::StringPiece& b) const OVERRIDE {
    size_t len = std::min(a.size(), b.size());
    return memcmp(a.begin(), b.begin(), len);
  }
  virtual const char* Name() const OVERRIDE { return "temp_comparator"; }
};

TEST(LevelDBDatabaseTest, CorruptionTest) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  const std::string key("key");
  const std::string value("value");
  std::string put_value;
  std::string got_value;
  SimpleComparator comparator;

  scoped_ptr<LevelDBDatabase> leveldb;
  LevelDBDatabase::Open(temp_directory.path(), &comparator, &leveldb);
  EXPECT_TRUE(leveldb);
  put_value = value;
  bool success = leveldb->Put(key, &put_value);
  EXPECT_TRUE(success);
  leveldb.Pass();
  EXPECT_FALSE(leveldb);

  LevelDBDatabase::Open(temp_directory.path(), &comparator, &leveldb);
  EXPECT_TRUE(leveldb);
  bool found = false;
  success = leveldb->Get(key, &got_value, &found);
  EXPECT_TRUE(success);
  EXPECT_TRUE(found);
  EXPECT_EQ(value, got_value);
  leveldb.Pass();
  EXPECT_FALSE(leveldb);

  base::FilePath file_path = temp_directory.path().AppendASCII("CURRENT");
  base::PlatformFile handle = base::CreatePlatformFile(
      file_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
      NULL,
      NULL);
  base::TruncatePlatformFile(handle, 0);
  base::ClosePlatformFile(handle);

  leveldb::Status status =
      LevelDBDatabase::Open(temp_directory.path(), &comparator, &leveldb);
  EXPECT_FALSE(leveldb);
  EXPECT_FALSE(status.ok());

  bool destroyed = LevelDBDatabase::Destroy(temp_directory.path());
  EXPECT_TRUE(destroyed);

  status = LevelDBDatabase::Open(temp_directory.path(), &comparator, &leveldb);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(leveldb);
  success = leveldb->Get(key, &got_value, &found);
  EXPECT_TRUE(success);
  EXPECT_FALSE(found);
}

TEST(LevelDBDatabaseTest, Transaction) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  const std::string key("key");
  std::string got_value;
  std::string put_value;
  SimpleComparator comparator;

  scoped_ptr<LevelDBDatabase> leveldb;
  LevelDBDatabase::Open(temp_directory.path(), &comparator, &leveldb);
  EXPECT_TRUE(leveldb);

  const std::string old_value("value");
  put_value = old_value;
  bool success = leveldb->Put(key, &put_value);
  EXPECT_TRUE(success);

  scoped_refptr<LevelDBTransaction> transaction =
      new LevelDBTransaction(leveldb.get());

  const std::string new_value("new value");
  put_value = new_value;
  success = leveldb->Put(key, &put_value);
  EXPECT_TRUE(success);

  bool found = false;
  success = transaction->Get(key, &got_value, &found);
  EXPECT_TRUE(success);
  EXPECT_TRUE(found);
  EXPECT_EQ(comparator.Compare(got_value, old_value), 0);

  found = false;
  success = leveldb->Get(key, &got_value, &found);
  EXPECT_TRUE(success);
  EXPECT_TRUE(found);
  EXPECT_EQ(comparator.Compare(got_value, new_value), 0);

  const std::string added_key("added key");
  const std::string added_value("added value");
  put_value = added_value;
  success = leveldb->Put(added_key, &put_value);
  EXPECT_TRUE(success);

  success = leveldb->Get(added_key, &got_value, &found);
  EXPECT_TRUE(success);
  EXPECT_TRUE(found);
  EXPECT_EQ(comparator.Compare(got_value, added_value), 0);

  success = transaction->Get(added_key, &got_value, &found);
  EXPECT_TRUE(success);
  EXPECT_FALSE(found);

  const std::string another_key("another key");
  const std::string another_value("another value");
  put_value = another_value;
  transaction->Put(another_key, &put_value);

  success = transaction->Get(another_key, &got_value, &found);
  EXPECT_TRUE(success);
  EXPECT_TRUE(found);
  EXPECT_EQ(comparator.Compare(got_value, another_value), 0);
}

TEST(LevelDBDatabaseTest, TransactionIterator) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  const std::string key1("key1");
  const std::string value1("value1");
  const std::string key2("key2");
  const std::string value2("value2");
  std::string put_value;
  SimpleComparator comparator;
  bool success;

  scoped_ptr<LevelDBDatabase> leveldb;
  LevelDBDatabase::Open(temp_directory.path(), &comparator, &leveldb);
  EXPECT_TRUE(leveldb);

  put_value = value1;
  success = leveldb->Put(key1, &put_value);
  EXPECT_TRUE(success);
  put_value = value2;
  success = leveldb->Put(key2, &put_value);
  EXPECT_TRUE(success);

  scoped_refptr<LevelDBTransaction> transaction =
      new LevelDBTransaction(leveldb.get());

  success = leveldb->Remove(key2);
  EXPECT_TRUE(success);

  scoped_ptr<LevelDBIterator> it = transaction->CreateIterator();

  it->Seek(std::string());

  EXPECT_TRUE(it->IsValid());
  EXPECT_EQ(comparator.Compare(it->Key(), key1), 0);
  EXPECT_EQ(comparator.Compare(it->Value(), value1), 0);

  it->Next();

  EXPECT_TRUE(it->IsValid());
  EXPECT_EQ(comparator.Compare(it->Key(), key2), 0);
  EXPECT_EQ(comparator.Compare(it->Value(), value2), 0);

  it->Next();

  EXPECT_FALSE(it->IsValid());
}

TEST(LevelDBDatabaseTest, TransactionCommitTest) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  const std::string key1("key1");
  const std::string key2("key2");
  const std::string value1("value1");
  const std::string value2("value2");
  const std::string value3("value3");

  std::string put_value;
  std::string got_value;
  SimpleComparator comparator;
  bool success;
  bool found;

  scoped_ptr<LevelDBDatabase> leveldb;
  LevelDBDatabase::Open(temp_directory.path(), &comparator, &leveldb);
  EXPECT_TRUE(leveldb);

  scoped_refptr<LevelDBTransaction> transaction =
      new LevelDBTransaction(leveldb.get());

  put_value = value1;
  transaction->Put(key1, &put_value);

  put_value = value2;
  transaction->Put(key2, &put_value);

  put_value = value3;
  transaction->Put(key2, &put_value);

  success = transaction->Commit();
  EXPECT_TRUE(success);

  success = leveldb->Get(key1, &got_value, &found);
  EXPECT_TRUE(success);
  EXPECT_TRUE(found);
  EXPECT_EQ(value1, got_value);

  success = leveldb->Get(key2, &got_value, &found);
  EXPECT_TRUE(success);
  EXPECT_TRUE(found);
  EXPECT_EQ(value3, got_value);
}

TEST(LevelDB, Locking) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  leveldb::Env* env = leveldb::IDBEnv();
  base::FilePath file = temp_directory.path().AppendASCII("LOCK");
  leveldb::FileLock* lock;
  leveldb::Status status = env->LockFile(file.AsUTF8Unsafe(), &lock);
  EXPECT_TRUE(status.ok());

  status = env->UnlockFile(lock);
  EXPECT_TRUE(status.ok());

  status = env->LockFile(file.AsUTF8Unsafe(), &lock);
  EXPECT_TRUE(status.ok());

  leveldb::FileLock* lock2;
  status = env->LockFile(file.AsUTF8Unsafe(), &lock2);
  EXPECT_FALSE(status.ok());

  status = env->UnlockFile(lock);
  EXPECT_TRUE(status.ok());
}

}  // namespace

}  // namespace content
