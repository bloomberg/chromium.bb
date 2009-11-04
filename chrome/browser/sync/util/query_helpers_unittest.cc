// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/query_helpers.h"

#include <limits>
#include <string>

#include "base/file_util.h"
#include "chrome/common/sqlite_utils.h"
#include "chrome/test/file_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::numeric_limits;
using std::string;

TEST(QueryHelpers, APEncode) {
  string test;
  char i;
  for (i = numeric_limits<char>::min(); i < numeric_limits<char>::max(); ++i)
    test.push_back(i);
  test.push_back(i);
  const string encoded = APEncode(test);
  const string decoded = APDecode(encoded);
  ASSERT_EQ(test, decoded);
}

TEST(QueryHelpers, TestExecFailure) {
  FilePath test_database;
  file_util::GetCurrentDirectory(&test_database);
  test_database = test_database.Append(
      FILE_PATH_LITERAL("queryhelper_test.sqlite3"));
  // Cleanup left-over file, if present.
  file_util::Delete(test_database, true);
  FileAutoDeleter file_deleter(test_database);
  {
    sqlite3* database = NULL;
    ASSERT_EQ(SQLITE_OK, SqliteOpen(test_database, &database));
    sqlite_utils::scoped_sqlite_db_ptr database_deleter(database);
    EXPECT_EQ(SQLITE_DONE, Exec(database, "CREATE TABLE test_table (idx int)"));
    EXPECT_NE(SQLITE_DONE, Exec(database, "ALTER TABLE test_table ADD COLUMN "
        "broken int32 default ?", -1));
  }
}
