// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/preprocessed/sqlite3.h"

class SQLStatementTest : public testing::Test {
 public:
  SQLStatementTest() {}

  void SetUp() {
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &path_));
    path_ = path_.AppendASCII("SQLStatementTest.db");
    file_util::Delete(path_, false);
    ASSERT_TRUE(db_.Init(path_));
  }

  void TearDown() {
    db_.Close();

    // If this fails something is going on with cleanup and later tests may
    // fail, so we want to identify problems right away.
    ASSERT_TRUE(file_util::Delete(path_, false));
  }

  sql::Connection& db() { return db_; }

 private:
  FilePath path_;
  sql::Connection db_;
};

TEST_F(SQLStatementTest, Assign) {
  sql::Statement s;
  EXPECT_FALSE(s);  // bool conversion operator.
  EXPECT_TRUE(!s);  // ! operator.
  EXPECT_FALSE(s.is_valid());

  s.Assign(db().GetUniqueStatement("CREATE TABLE foo (a, b)"));
  EXPECT_TRUE(s);
  EXPECT_FALSE(!s);
  EXPECT_TRUE(s.is_valid());
}

TEST_F(SQLStatementTest, Run) {
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (3, 12)"));

  sql::Statement s(db().GetUniqueStatement("SELECT b FROM foo WHERE a=?"));
  EXPECT_FALSE(s.Succeeded());

  // Stepping it won't work since we haven't bound the value.
  EXPECT_FALSE(s.Step());

  // Run should fail since this produces output, and we should use Step(). This
  // gets a bit wonky since sqlite says this is OK so succeeded is set.
  s.Reset();
  s.BindInt(0, 3);
  EXPECT_FALSE(s.Run());
  EXPECT_EQ(SQLITE_ROW, db().GetErrorCode());
  EXPECT_TRUE(s.Succeeded());

  // Resetting it should put it back to the previous state (not runnable).
  s.Reset();
  EXPECT_FALSE(s.Succeeded());

  // Binding and stepping should produce one row.
  s.BindInt(0, 3);
  EXPECT_TRUE(s.Step());
  EXPECT_TRUE(s.Succeeded());
  EXPECT_EQ(12, s.ColumnInt(0));
  EXPECT_FALSE(s.Step());
  EXPECT_TRUE(s.Succeeded());
}
