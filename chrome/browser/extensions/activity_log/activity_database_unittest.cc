// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/time.h"
#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "chrome/browser/extensions/activity_log/api_actions.h"
#include "chrome/browser/extensions/activity_log/blocked_actions.h"
#include "chrome/browser/extensions/activity_log/dom_actions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/dom_action_types.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/chromeos_switches.h"
#endif

using content::BrowserThread;

namespace extensions {

class ActivityDatabaseTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined OS_CHROMEOS
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    CommandLine command_line(CommandLine::NO_PROGRAM);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogTesting);
  }

  virtual void TearDown() OVERRIDE {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
#if defined OS_CHROMEOS
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif

};


// Check that the database is initialized properly.
TEST_F(ActivityDatabaseTest, Init) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityInit.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->Init(db_file);
  ASSERT_TRUE(activity_db->is_db_valid());
  activity_db->Close();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));
  ASSERT_TRUE(db.DoesTableExist(DOMAction::kTableName));
  ASSERT_TRUE(db.DoesTableExist(APIAction::kTableName));
  ASSERT_TRUE(db.DoesTableExist(BlockedAction::kTableName));
  db.Close();
}

// Check that API actions are recorded in the db.
TEST_F(ActivityDatabaseTest, RecordAPIAction) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->Init(db_file);
  activity_db->SetBatchModeForTesting(false);
  ASSERT_TRUE(activity_db->is_db_valid());
  scoped_refptr<APIAction> action = new APIAction(
      "punky",
      base::Time::Now(),
      APIAction::CALL,
      "brewster",
      "woof",
      "extra");
  activity_db->RecordAction(action);
  activity_db->Close();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));

  ASSERT_TRUE(db.DoesTableExist(APIAction::kTableName));
  std::string sql_str = "SELECT * FROM " +
      std::string(APIAction::kTableName);
  sql::Statement statement(db.GetUniqueStatement(sql_str.c_str()));
  ASSERT_TRUE(statement.Step());
  ASSERT_EQ("punky", statement.ColumnString(0));
  ASSERT_EQ(0, statement.ColumnInt(2));
  ASSERT_EQ("brewster", statement.ColumnString(3));
  ASSERT_EQ("woof", statement.ColumnString(4));
}

// Check that DOM actions are recorded in the db.
TEST_F(ActivityDatabaseTest, RecordDOMAction) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->Init(db_file);
  activity_db->SetBatchModeForTesting(false);
  ASSERT_TRUE(activity_db->is_db_valid());
  scoped_refptr<DOMAction> action = new DOMAction(
      "punky",
      base::Time::Now(),
      DomActionType::MODIFIED,
      GURL("http://www.google.com/foo?bar"),
      string16(),
      "lets",
      "vamoose",
      "extra");
  activity_db->RecordAction(action);
  activity_db->Close();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));

  ASSERT_TRUE(db.DoesTableExist(APIAction::kTableName));
  std::string sql_str = "SELECT * FROM " +
      std::string(DOMAction::kTableName);
  sql::Statement statement(db.GetUniqueStatement(sql_str.c_str()));
  ASSERT_TRUE(statement.Step());
  ASSERT_EQ("punky", statement.ColumnString(0));
  ASSERT_EQ(DomActionType::MODIFIED, statement.ColumnInt(2));
  ASSERT_EQ("http://www.google.com", statement.ColumnString(3));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExtensionActivityLogTesting))
    ASSERT_EQ("/foo?bar", statement.ColumnString(4));
  else
    ASSERT_EQ("/foo", statement.ColumnString(4));
  ASSERT_EQ("lets", statement.ColumnString(6));
  ASSERT_EQ("vamoose", statement.ColumnString(7));
  ASSERT_EQ("extra", statement.ColumnString(8));
}

// Check that blocked actions are recorded in the db.
TEST_F(ActivityDatabaseTest, RecordBlockedAction) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->Init(db_file);
  ASSERT_TRUE(activity_db->is_db_valid());
  scoped_refptr<BlockedAction> action = new BlockedAction(
      "punky",
      base::Time::Now(),
      "do.evilThings",
      "1, 2",
      BlockedAction::ACCESS_DENIED,
      "extra");
  activity_db->RecordAction(action);
  activity_db->Close();

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
  ASSERT_EQ(1, statement.ColumnInt(4));
  ASSERT_EQ("extra", statement.ColumnString(5));
}

// Check that we can read back recent actions in the db.
TEST_F(ActivityDatabaseTest, GetTodaysActions) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));

  // Record some actions
  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->Init(db_file);
  ASSERT_TRUE(activity_db->is_db_valid());
  scoped_refptr<APIAction> api_action = new APIAction(
      "punky",
      mock_clock.Now() - base::TimeDelta::FromMinutes(40),
      APIAction::CALL,
      "brewster",
      "woof",
      "extra");
  scoped_refptr<DOMAction> dom_action = new DOMAction(
      "punky",
      mock_clock.Now(),
      DomActionType::MODIFIED,
      GURL("http://www.google.com"),
      string16(),
      "lets",
      "vamoose",
      "extra");
  scoped_refptr<DOMAction> extra_dom_action = new DOMAction(
      "scoobydoo",
      mock_clock.Now(),
      DomActionType::MODIFIED,
      GURL("http://www.google.com"),
      string16(),
      "lets",
      "vamoose",
      "extra");
  activity_db->RecordAction(api_action);
  activity_db->RecordAction(dom_action);
  activity_db->RecordAction(extra_dom_action);

  // Read them back
  std::string api_print = "ID: punky, CATEGORY: call, "
      "API: brewster, ARGS: woof";
  std::string dom_print = "DOM API CALL: lets, ARGS: vamoose, VERB: modified";
  scoped_ptr<std::vector<scoped_refptr<Action> > > actions =
      activity_db->GetActions("punky", 0);
  ASSERT_EQ(2, static_cast<int>(actions->size()));
  ASSERT_EQ(dom_print, actions->at(0)->PrintForDebug());
  ASSERT_EQ(api_print, actions->at(1)->PrintForDebug());

  activity_db->Close();
}

// Check that we can read back recent actions in the db.
TEST_F(ActivityDatabaseTest, GetOlderActions) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));

  // Record some actions
  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->Init(db_file);
  ASSERT_TRUE(activity_db->is_db_valid());
  scoped_refptr<APIAction> api_action = new APIAction(
      "punky",
      mock_clock.Now() - base::TimeDelta::FromDays(3)
          - base::TimeDelta::FromMinutes(40),
      APIAction::CALL,
      "brewster",
      "woof",
      "extra");
  scoped_refptr<DOMAction> dom_action = new DOMAction(
      "punky",
      mock_clock.Now() - base::TimeDelta::FromDays(3),
      DomActionType::MODIFIED,
      GURL("http://www.google.com"),
      string16(),
      "lets",
      "vamoose",
      "extra");
  scoped_refptr<DOMAction> toonew_dom_action = new DOMAction(
      "punky",
      mock_clock.Now(),
      DomActionType::MODIFIED,
      GURL("http://www.google.com"),
      string16(),
      "too new",
      "vamoose",
      "extra");
  scoped_refptr<DOMAction> tooold_dom_action = new DOMAction(
      "punky",
      mock_clock.Now() - base::TimeDelta::FromDays(7),
      DomActionType::MODIFIED,
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
  std::string api_print = "ID: punky, CATEGORY: call, "
      "API: brewster, ARGS: woof";
  std::string dom_print = "DOM API CALL: lets, ARGS: vamoose, VERB: modified";
  scoped_ptr<std::vector<scoped_refptr<Action> > > actions =
      activity_db->GetActions("punky", 3);
  ASSERT_EQ(2, static_cast<int>(actions->size()));
  ASSERT_EQ(dom_print, actions->at(0)->PrintForDebug());
  ASSERT_EQ(api_print, actions->at(1)->PrintForDebug());

  activity_db->Close();
}

TEST_F(ActivityDatabaseTest, BatchModeOff) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));

  // Record some actions
  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->Init(db_file);
  activity_db->SetBatchModeForTesting(false);
  activity_db->SetClockForTesting(&mock_clock);
  ASSERT_TRUE(activity_db->is_db_valid());
  scoped_refptr<APIAction> api_action = new APIAction(
      "punky",
      mock_clock.Now() - base::TimeDelta::FromMinutes(40),
      APIAction::CALL,
      "brewster",
      "woof",
      "extra");
  activity_db->RecordAction(api_action);

  scoped_ptr<std::vector<scoped_refptr<Action> > > actions =
      activity_db->GetActions("punky", 0);
  ASSERT_EQ(1, static_cast<int>(actions->size()));
  activity_db->Close();
}

TEST_F(ActivityDatabaseTest, BatchModeOn) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  // Use a mock clock to set the time, and a special timer to control the
  // timing and skip ahead in time.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(11));

  // Record some actions
  ActivityDatabase* activity_db = new ActivityDatabase();
  activity_db->Init(db_file);
  activity_db->SetBatchModeForTesting(true);
  activity_db->SetClockForTesting(&mock_clock);
  ASSERT_TRUE(activity_db->is_db_valid());
  scoped_refptr<APIAction> api_action = new APIAction(
      "punky",
      mock_clock.Now() - base::TimeDelta::FromMinutes(40),
      APIAction::CALL,
      "brewster",
      "woof",
      "extra");
  activity_db->RecordAction(api_action);

  scoped_ptr<std::vector<scoped_refptr<Action> > > actions_before =
      activity_db->GetActions("punky", 0);
  ASSERT_EQ(0, static_cast<int>(actions_before->size()));

  // Artificially trigger and then stop the timer.
  activity_db->SetTimerForTesting(0);
  base::MessageLoop::current()->RunUntilIdle();

  scoped_ptr<std::vector<scoped_refptr<Action> > > actions_after =
        activity_db->GetActions("punky", 0);
    ASSERT_EQ(1, static_cast<int>(actions_after->size()));

  activity_db->Close();
}

// Check that nothing explodes if the DB isn't initialized.
TEST_F(ActivityDatabaseTest, InitFailure) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  file_util::Delete(db_file, false);

  ActivityDatabase* activity_db = new ActivityDatabase();
  scoped_refptr<APIAction> action = new APIAction(
      "punky",
      base::Time::Now(),
      APIAction::CALL,
      "brewster",
      "woooof",
      "extra");
  activity_db->RecordAction(action);
  activity_db->Close();
}

}  // namespace extensions

