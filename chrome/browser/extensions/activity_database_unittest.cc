// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/extensions/activity_database.h"
#include "chrome/browser/extensions/api_actions.h"
#include "chrome/browser/extensions/blocked_actions.h"
#include "chrome/browser/extensions/dom_actions.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Check that the database is initialized properly.
TEST(ActivityDatabaseTest, Init) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityInit.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->AddRef();
  activity_db->Init(db_file);
  ASSERT_TRUE(activity_db->initialized());
  activity_db->Release();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));
  ASSERT_TRUE(db.DoesTableExist(DOMAction::kTableName));
  ASSERT_TRUE(db.DoesTableExist(APIAction::kTableName));
  ASSERT_TRUE(db.DoesTableExist(BlockedAction::kTableName));
  db.Close();
}

// Check that API actions are recorded in the db.
TEST(ActivityDatabaseTest, RecordAPIAction) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->AddRef();
  activity_db->Init(db_file);
  ASSERT_TRUE(activity_db->initialized());
  scoped_refptr<APIAction> action = new APIAction(
      "punky",
      base::Time::Now(),
      APIAction::CALL,
      APIAction::READ,
      APIAction::BOOKMARK,
      "brewster",
      "woof",
      "extra");
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
  ASSERT_EQ("CALL", statement.ColumnString(2));
  ASSERT_EQ("READ", statement.ColumnString(3));
  ASSERT_EQ("BOOKMARK", statement.ColumnString(4));
  ASSERT_EQ("brewster", statement.ColumnString(5));
  ASSERT_EQ("woof", statement.ColumnString(6));
}

// Check that blocked actions are recorded in the db.
TEST(ActivityDatabaseTest, RecordBlockedAction) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->AddRef();
  activity_db->Init(db_file);
  ASSERT_TRUE(activity_db->initialized());
  scoped_refptr<BlockedAction> action = new BlockedAction(
      "punky",
      base::Time::Now(),
      "do.evilThings",
      "1, 2",
      "because i said so",
      "extra");
  activity_db->RecordAction(action);
  activity_db->Close();
  activity_db->Release();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));

  ASSERT_TRUE(db.DoesTableExist(BlockedAction::kTableName));
  std::string sql_str = "SELECT * FROM " +
      std::string(BlockedAction::kTableName);
  sql::Statement statement(db.GetUniqueStatement(sql_str.c_str()));
  ASSERT_TRUE(statement.Step());
  ASSERT_EQ("punky", statement.ColumnString(0));
  ASSERT_EQ("do.evilThings", statement.ColumnString(2));
  ASSERT_EQ("1, 2", statement.ColumnString(3));
  ASSERT_EQ("because i said so", statement.ColumnString(4));
  ASSERT_EQ("extra", statement.ColumnString(5));
}

// Check that we can read back recent actions in the db.
TEST(ActivityDatabaseTest, GetTodaysActions) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  // Record some actions
  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->AddRef();
  activity_db->Init(db_file);
  ASSERT_TRUE(activity_db->initialized());
  scoped_refptr<APIAction> api_action = new APIAction(
      "punky",
      base::Time::Now() - base::TimeDelta::FromMinutes(40),
      APIAction::CALL,
      APIAction::READ,
      APIAction::BOOKMARK,
      "brewster",
      "woof",
      "extra");
  scoped_refptr<DOMAction> dom_action = new DOMAction(
      "punky",
      base::Time::Now(),
      DOMAction::MODIFIED,
      GURL("http://www.google.com"),
      string16(),
      "lets",
      "vamoose",
      "extra");
  scoped_refptr<DOMAction> extra_dom_action = new DOMAction(
      "scoobydoo",
      base::Time::Now(),
      DOMAction::MODIFIED,
      GURL("http://www.google.com"),
      string16(),
      "lets",
      "vamoose",
      "extra");
  activity_db->RecordAction(api_action);
  activity_db->RecordAction(dom_action);
  activity_db->RecordAction(extra_dom_action);

  // Read them back
  std::string api_print = "ID: punky, CATEGORY: CALL, VERB: READ, TARGET: "
      "BOOKMARK, API: brewster, ARGS: woof";
  std::string dom_print = "DOM API CALL: lets, ARGS: vamoose";
  scoped_ptr<std::vector<scoped_refptr<Action> > > actions =
      activity_db->GetActions("punky", 0);
  ASSERT_EQ(2, static_cast<int>(actions->size()));
  ASSERT_EQ(dom_print, actions->at(0)->PrettyPrintForDebug());
  ASSERT_EQ(api_print, actions->at(1)->PrettyPrintForDebug());

  activity_db->Close();
  activity_db->Release();
}

// Check that we can read back recent actions in the db.
TEST(ActivityDatabaseTest, GetOlderActions) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  // Record some actions
  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->AddRef();
  activity_db->Init(db_file);
  ASSERT_TRUE(activity_db->initialized());
  scoped_refptr<APIAction> api_action = new APIAction(
      "punky",
      base::Time::Now() - base::TimeDelta::FromDays(3)
          - base::TimeDelta::FromMinutes(40),
      APIAction::CALL,
      APIAction::READ,
      APIAction::BOOKMARK,
      "brewster",
      "woof",
      "extra");
  scoped_refptr<DOMAction> dom_action = new DOMAction(
      "punky",
      base::Time::Now() - base::TimeDelta::FromDays(3),
      DOMAction::MODIFIED,
      GURL("http://www.google.com"),
      string16(),
      "lets",
      "vamoose",
      "extra");
  scoped_refptr<DOMAction> toonew_dom_action = new DOMAction(
      "punky",
      base::Time::Now(),
      DOMAction::MODIFIED,
      GURL("http://www.google.com"),
      string16(),
      "too new",
      "vamoose",
      "extra");
  scoped_refptr<DOMAction> tooold_dom_action = new DOMAction(
      "punky",
      base::Time::Now() - base::TimeDelta::FromDays(7),
      DOMAction::MODIFIED,
      GURL("http://www.google.com"),
      string16(),
      "too old",
      "vamoose",
      "extra");
  activity_db->RecordAction(api_action);
  activity_db->RecordAction(dom_action);
  activity_db->RecordAction(toonew_dom_action);
  activity_db->RecordAction(tooold_dom_action);

  // Read them back
  std::string api_print = "ID: punky, CATEGORY: CALL, VERB: READ, TARGET: "
      "BOOKMARK, API: brewster, ARGS: woof";
  std::string dom_print = "DOM API CALL: lets, ARGS: vamoose";
  scoped_ptr<std::vector<scoped_refptr<Action> > > actions =
      activity_db->GetActions("punky", 3);
  ASSERT_EQ(2, static_cast<int>(actions->size()));
  ASSERT_EQ(dom_print, actions->at(0)->PrettyPrintForDebug());
  ASSERT_EQ(api_print, actions->at(1)->PrettyPrintForDebug());

  activity_db->Close();
  activity_db->Release();
}

// Check that nothing explodes if the DB isn't initialized.
TEST(ActivityDatabaseTest, InitFailure) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->AddRef();
  scoped_refptr<APIAction> action = new APIAction(
      "punky",
      base::Time::Now(),
      APIAction::CALL,
      APIAction::READ,
      APIAction::BOOKMARK,
      "brewster",
      "woooof",
      "extra");
  activity_db->RecordAction(action);
  activity_db->Close();
  activity_db->Release();
}

}  // namespace

