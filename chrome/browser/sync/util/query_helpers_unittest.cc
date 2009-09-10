// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/query_helpers.h"

#include <limits>
#include <string>

#include "chrome/browser/sync/util/compat-file.h"
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
  sqlite3* database;
  const PathString test_database(PSTR("queryhelper_test.sqlite3"));
  PathRemove(test_database);
  ASSERT_EQ(SQLITE_OK, SqliteOpen(test_database, &database));
  EXPECT_EQ(SQLITE_DONE, Exec(database, "CREATE TABLE test_table (idx int)"));
  EXPECT_NE(SQLITE_DONE, Exec(database, "ALTER TABLE test_table ADD COLUMN "
      "broken int32 default ?", -1));
  PathRemove(test_database);
}
