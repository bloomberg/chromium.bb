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
#include "base/time/time.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
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
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/chromeos_switches.h"
#endif

using content::BrowserThread;

namespace constants = activity_log_constants;

namespace extensions {

// A dummy implementation of ActivityDatabase::Delegate, sufficient for
// the unit tests.
class ActivityDatabaseTestPolicy : public ActivityDatabase::Delegate {
 public:
  ActivityDatabaseTestPolicy() {};

 protected:
  virtual bool OnDatabaseInit(sql::Connection* db) OVERRIDE {
    return ActivityDatabase::InitializeTable(
        db,
        FullStreamUIPolicy::kTableName,
        FullStreamUIPolicy::kTableContentFields,
        FullStreamUIPolicy::kTableFieldTypes,
        FullStreamUIPolicy::kTableFieldCount);
  }

  // Called by ActivityDatabase just before the ActivityDatabase object is
  // deleted.  The database will make no further callbacks after invoking this
  // method, so it is an appropriate time for the policy to delete itself.
  virtual void OnDatabaseClose() OVERRIDE {
    delete this;
  }
};

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

  // Creates a test database and initializes the table schema.
  ActivityDatabase* OpenDatabase(const base::FilePath& db_file) const {
    ActivityDatabase* activity_db =
        new ActivityDatabase(new ActivityDatabaseTestPolicy());
    activity_db->Init(db_file);
    CHECK(activity_db->is_db_valid());
    return activity_db;
  }

 private:
#if defined OS_CHROMEOS
  chromeos::ScopedStubNetworkLibraryEnabler stub_network_library_enabler_;
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
  base::DeleteFile(db_file, false);

  ActivityDatabase* activity_db = OpenDatabase(db_file);
  activity_db->Close();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));
  ASSERT_TRUE(db.DoesTableExist(FullStreamUIPolicy::kTableName));
  db.Close();
}

// Check that actions are recorded in the db.
TEST_F(ActivityDatabaseTest, RecordAction) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  base::DeleteFile(db_file, false);

  ActivityDatabase* activity_db = OpenDatabase(db_file);
  activity_db->SetBatchModeForTesting(false);
  scoped_refptr<Action> action(new Action(
      "punky", base::Time::Now(), Action::ACTION_API_CALL, "brewster"));
  action->mutable_args()->AppendString("woof");
  action->set_page_url(GURL("http://www.google.com/"));
  action->mutable_other()->SetString(constants::kActionExtra, "extra");
  activity_db->RecordAction(action);
  activity_db->Close();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));

  ASSERT_TRUE(db.DoesTableExist(FullStreamUIPolicy::kTableName));
  std::string sql_str = "SELECT * FROM " +
      std::string(FullStreamUIPolicy::kTableName);
  sql::Statement statement(db.GetUniqueStatement(sql_str.c_str()));
  ASSERT_TRUE(statement.Step());
  ASSERT_EQ("punky", statement.ColumnString(0));
  ASSERT_EQ(static_cast<int>(Action::ACTION_API_CALL), statement.ColumnInt(2));
  ASSERT_EQ("brewster", statement.ColumnString(3));
  ASSERT_EQ("[\"woof\"]", statement.ColumnString(4));
  ASSERT_EQ("http://www.google.com/", statement.ColumnString(5));
  ASSERT_EQ("{\"extra\":\"extra\"}", statement.ColumnString(8));
}

// Check that we can read back recent actions in the db.
TEST_F(ActivityDatabaseTest, GetTodaysActions) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  base::DeleteFile(db_file, false);

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));

  // Record some actions
  ActivityDatabase* activity_db = OpenDatabase(db_file);

  scoped_refptr<Action> action;
  action = new Action("punky",
                      mock_clock.Now() - base::TimeDelta::FromMinutes(40),
                      Action::ACTION_API_CALL,
                      "brewster");
  action->mutable_args()->AppendString("woof");
  activity_db->RecordAction(action);

  action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  activity_db->RecordAction(action);

  action = new Action(
      "scoobydoo", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  activity_db->RecordAction(action);

  // Read them back
  std::string api_print =
      "ID=punky CATEGORY=api_call API=brewster ARGS=[\"woof\"]";
  std::string dom_print =
      "ID=punky CATEGORY=dom_access API=lets ARGS=[\"vamoose\"] "
      "PAGE_URL=http://www.google.com/";
  scoped_ptr<std::vector<scoped_refptr<Action> > > actions =
      activity_db->GetActions("punky", 0);
  ASSERT_EQ(2, static_cast<int>(actions->size()));
  ASSERT_EQ(dom_print, actions->at(0)->PrintForDebug());
  ASSERT_EQ(api_print, actions->at(1)->PrintForDebug());

  activity_db->Close();
}

// Check that we can read back less recent actions in the db.
TEST_F(ActivityDatabaseTest, GetOlderActions) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  base::DeleteFile(db_file, false);

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));

  // Record some actions
  ActivityDatabase* activity_db = OpenDatabase(db_file);
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock.Now() - base::TimeDelta::FromDays(3) -
                     base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL,
                 "brewster");
  action->mutable_args()->AppendString("woof");
  activity_db->RecordAction(action);

  action = new Action("punky",
                      mock_clock.Now() - base::TimeDelta::FromDays(3),
                      Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  activity_db->RecordAction(action);

  action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("too new");
  action->set_page_url(GURL("http://www.google.com"));
  activity_db->RecordAction(action);

  action = new Action("punky",
                      mock_clock.Now() - base::TimeDelta::FromDays(7),
                      Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args()->AppendString("too old");
  action->set_page_url(GURL("http://www.google.com"));
  activity_db->RecordAction(action);

  // Read them back
  std::string api_print =
      "ID=punky CATEGORY=api_call API=brewster ARGS=[\"woof\"]";
  std::string dom_print =
      "ID=punky CATEGORY=dom_access API=lets ARGS=[\"vamoose\"] "
      "PAGE_URL=http://www.google.com/";
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
  base::DeleteFile(db_file, false);

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));

  // Record some actions
  ActivityDatabase* activity_db = OpenDatabase(db_file);
  activity_db->SetBatchModeForTesting(false);
  activity_db->SetClockForTesting(&mock_clock);
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock.Now() - base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL,
                 "brewster");
  action->mutable_args()->AppendString("woof");
  activity_db->RecordAction(action);

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
  base::DeleteFile(db_file, false);

  // Use a mock clock to set the time, and a special timer to control the
  // timing and skip ahead in time.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(11));

  // Record some actions
  ActivityDatabase* activity_db = OpenDatabase(db_file);
  activity_db->SetBatchModeForTesting(true);
  activity_db->SetClockForTesting(&mock_clock);
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock.Now() - base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL,
                 "brewster");
  action->mutable_args()->AppendString("woof");
  activity_db->RecordAction(action);

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
  base::DeleteFile(db_file, false);

  ActivityDatabase* activity_db =
      new ActivityDatabase(new ActivityDatabaseTestPolicy());
  scoped_refptr<Action> action = new Action(
      "punky", base::Time::Now(), Action::ACTION_API_CALL, "brewster");
  action->mutable_args()->AppendString("woof");
  activity_db->RecordAction(action);
  activity_db->Close();
}

}  // namespace extensions
