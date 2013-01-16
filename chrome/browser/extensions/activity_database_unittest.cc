// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/extensions/activity_database.h"
#include "chrome/browser/extensions/api_actions.h"
#include "chrome/browser/extensions/blocked_actions.h"
#include "chrome/browser/extensions/url_actions.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Check that the database is initialized properly.
TEST(ActivityDatabaseTest, Init) {
  base::ScopedTempDir temp_dir;
  FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityInit.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->AddRef();
  activity_db->Init(db_file, NULL);
  ASSERT_TRUE(activity_db->initialized());
  activity_db->Release();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));
  ASSERT_TRUE(db.DoesTableExist(UrlAction::kTableName));
  ASSERT_TRUE(db.DoesTableExist(APIAction::kTableName));
  ASSERT_TRUE(db.DoesTableExist(BlockedAction::kTableName));
  db.Close();
}

// Check that actions are recorded in the db.
TEST(ActivityDatabaseTest, RecordAction) {
  base::ScopedTempDir temp_dir;
  FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->AddRef();
  activity_db->Init(db_file, NULL);
  ASSERT_TRUE(activity_db->initialized());
  scoped_refptr<APIAction> action = new APIAction(
      "punky",
      base::Time::Now(),
      APIAction::READ,
      APIAction::BOOKMARK,
      "brewster",
      "");
  activity_db->RecordAction(action);
  activity_db->Close();
  activity_db->Release();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));

  ASSERT_TRUE(db.DoesTableExist(APIAction::kTableName));
  std::string sql_str = "SELECT * FROM " +
      std::string(APIAction::kTableName);
  sql::Statement statement(db.GetUniqueStatement(sql_str.c_str()));
  ASSERT_TRUE(statement.Step());
  ASSERT_EQ("punky", statement.ColumnString(0));
  ASSERT_EQ("READ", statement.ColumnString(2));
  ASSERT_EQ("BOOKMARK", statement.ColumnString(3));
  ASSERT_EQ("brewster", statement.ColumnString(4));
}

// Check that nothing explodes if the DB isn't initialized.
TEST(ActivityDatabaseTest, InitFailure) {
  base::ScopedTempDir temp_dir;
  FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->AddRef();
  scoped_refptr<APIAction> action = new APIAction(
      "punky",
      base::Time::Now(),
      APIAction::READ,
      APIAction::BOOKMARK,
      "brewster",
      "");
  activity_db->RecordAction(action);
  activity_db->Close();
  activity_db->Release();
}

}  // namespace
