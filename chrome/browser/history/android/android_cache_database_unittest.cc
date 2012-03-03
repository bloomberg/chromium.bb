// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/history/android/android_cache_database.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

class SimpleAndroidCacheDatabase : public AndroidCacheDatabase {
 public:
  explicit SimpleAndroidCacheDatabase(sql::Connection* connection)
      :db_(connection) {
  }

 protected:
  sql::Connection& GetDB() {
    return *db_;
  }

 private:
  sql::Connection* db_;
};

class AndroidCacheDatabaseTest : public testing::Test {
 public:
  AndroidCacheDatabaseTest() {
  }
  ~AndroidCacheDatabaseTest() {
  }

 protected:
  virtual void SetUp() {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    main_db_name_ = temp_dir_.path().AppendASCII("history.db");
    android_cache_db_name_ = temp_dir_.path().AppendASCII(
        "TestAndroidCache.db");
  }

  ScopedTempDir temp_dir_;
  FilePath android_cache_db_name_;
  FilePath main_db_name_;
};

TEST_F(AndroidCacheDatabaseTest, InitAndroidCacheDatabase) {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(main_db_name_));
  SimpleAndroidCacheDatabase android_cache_db(&connection);
  EXPECT_EQ(sql::INIT_OK,
            android_cache_db.InitAndroidCacheDatabase(android_cache_db_name_));
  // Try to run a sql against the table to verify it exists.
  EXPECT_TRUE(connection.Execute(
      "DELETE FROM android_cache_db.bookmark_cache"));
}

}  // namespace history
