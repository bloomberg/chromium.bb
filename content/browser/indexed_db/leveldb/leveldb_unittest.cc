// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_piece.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content {

namespace {

class SimpleComparator : public LevelDBComparator {
 public:
  int Compare(const base::StringPiece& a,
              const base::StringPiece& b) const override {
    size_t len = std::min(a.size(), b.size());
    return memcmp(a.begin(), b.begin(), len);
  }
  const char* Name() const override { return "temp_comparator"; }
};

}  // namespace

TEST(LevelDBDatabaseTest, CorruptionTest) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  const std::string key("key");
  const std::string value("value");
  std::string put_value;
  std::string got_value;
  SimpleComparator comparator;

  std::unique_ptr<LevelDBDatabase> leveldb;
  LevelDBDatabase::Open(temp_directory.GetPath(), &comparator, &leveldb);
  EXPECT_TRUE(leveldb);
  put_value = value;
  leveldb::Status status = leveldb->Put(key, &put_value);
  EXPECT_TRUE(status.ok());
  leveldb.reset();
  EXPECT_FALSE(leveldb);

  LevelDBDatabase::Open(temp_directory.GetPath(), &comparator, &leveldb);
  EXPECT_TRUE(leveldb);
  bool found = false;
  status = leveldb->Get(key, &got_value, &found);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(found);
  EXPECT_EQ(value, got_value);
  leveldb.reset();
  EXPECT_FALSE(leveldb);

  base::FilePath file_path = temp_directory.GetPath().AppendASCII("CURRENT");
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  file.SetLength(0);
  file.Close();

  status =
      LevelDBDatabase::Open(temp_directory.GetPath(), &comparator, &leveldb);
  EXPECT_FALSE(leveldb);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(status.IsCorruption());

  status = LevelDBDatabase::Destroy(temp_directory.GetPath());
  EXPECT_TRUE(status.ok());

  status =
      LevelDBDatabase::Open(temp_directory.GetPath(), &comparator, &leveldb);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(leveldb);
  status = leveldb->Get(key, &got_value, &found);
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(found);
}

TEST(LevelDB, Locking) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  leveldb::Env* env = LevelDBEnv::Get();
  base::FilePath file = temp_directory.GetPath().AppendASCII("LOCK");
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

}  // namespace content
