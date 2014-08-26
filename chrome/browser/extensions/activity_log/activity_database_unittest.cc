// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
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
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/dom_action_types.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
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

  static const char kTableName[];
  static const char* kTableContentFields[];
  static const char* kTableFieldTypes[];

  virtual void Record(ActivityDatabase* db, scoped_refptr<Action> action);

 protected:
  virtual bool InitDatabase(sql::Connection* db) OVERRIDE;
  virtual bool FlushDatabase(sql::Connection*) OVERRIDE;
  virtual void OnDatabaseFailure() OVERRIDE {}
  virtual void OnDatabaseClose() OVERRIDE { delete this; }

  std::vector<scoped_refptr<Action> > queue_;
};

const char ActivityDatabaseTestPolicy::kTableName[] = "actions";
const char* ActivityDatabaseTestPolicy::kTableContentFields[] = {
    "extension_id", "time", "action_type", "api_name"};
const char* ActivityDatabaseTestPolicy::kTableFieldTypes[] = {
    "LONGVARCHAR NOT NULL", "INTEGER", "INTEGER", "LONGVARCHAR"};

bool ActivityDatabaseTestPolicy::InitDatabase(sql::Connection* db) {
  return ActivityDatabase::InitializeTable(db,
                                           kTableName,
                                           kTableContentFields,
                                           kTableFieldTypes,
                                           arraysize(kTableContentFields));
}

bool ActivityDatabaseTestPolicy::FlushDatabase(sql::Connection* db) {
  std::string sql_str =
      "INSERT INTO " + std::string(kTableName) +
      " (extension_id, time, action_type, api_name) VALUES (?,?,?,?)";

  std::vector<scoped_refptr<Action> >::size_type i;
  for (i = 0; i < queue_.size(); i++) {
    const Action& action = *queue_[i].get();
    sql::Statement statement(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
    statement.BindString(0, action.extension_id());
    statement.BindInt64(1, action.time().ToInternalValue());
    statement.BindInt(2, static_cast<int>(action.action_type()));
    statement.BindString(3, action.api_name());
    if (!statement.Run()) {
      LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
      return false;
    }
  }

  queue_.clear();
  return true;
}

void ActivityDatabaseTestPolicy::Record(ActivityDatabase* db,
                                        scoped_refptr<Action> action) {
  queue_.push_back(action);
  db->AdviseFlush(queue_.size());
}

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
  ActivityDatabase* OpenDatabase(const base::FilePath& db_file) {
    db_delegate_ = new ActivityDatabaseTestPolicy();
    ActivityDatabase* activity_db = new ActivityDatabase(db_delegate_);
    activity_db->Init(db_file);
    CHECK(activity_db->is_db_valid());
    return activity_db;
  }

  scoped_refptr<Action> CreateAction(const base::Time& time,
                                     const std::string& api_name) const {
    scoped_refptr<Action> action(
        new Action("punky", time, Action::ACTION_API_CALL, api_name));
    return action;
  }

  void Record(ActivityDatabase* db, scoped_refptr<Action> action) {
    db_delegate_->Record(db, action);
  }

  int CountActions(sql::Connection* db, const std::string& api_name_pattern) {
    if (!db->DoesTableExist(ActivityDatabaseTestPolicy::kTableName))
      return -1;
    std::string sql_str = "SELECT COUNT(*) FROM " +
                          std::string(ActivityDatabaseTestPolicy::kTableName) +
                          " WHERE api_name LIKE ?";
    sql::Statement statement(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
    statement.BindString(0, api_name_pattern);
    if (!statement.Step())
      return -1;
    return statement.ColumnInt(0);
  }

 private:
#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif

  ActivityDatabaseTestPolicy* db_delegate_;
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
  ASSERT_TRUE(db.DoesTableExist(ActivityDatabaseTestPolicy::kTableName));
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
  scoped_refptr<Action> action = CreateAction(base::Time::Now(), "brewster");
  Record(activity_db, action);
  activity_db->Close();

  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));

  ASSERT_EQ(1, CountActions(&db, "brewster"));
}

TEST_F(ActivityDatabaseTest, BatchModeOff) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  base::DeleteFile(db_file, false);

  // Record some actions
  ActivityDatabase* activity_db = OpenDatabase(db_file);
  activity_db->SetBatchModeForTesting(false);

  scoped_refptr<Action> action = CreateAction(base::Time::Now(), "brewster");
  Record(activity_db, action);
  ASSERT_EQ(1, CountActions(&activity_db->db_, "brewster"));

  activity_db->Close();
}

TEST_F(ActivityDatabaseTest, BatchModeOn) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  base::DeleteFile(db_file, false);

  // Record some actions
  ActivityDatabase* activity_db = OpenDatabase(db_file);
  activity_db->SetBatchModeForTesting(true);
  scoped_refptr<Action> action = CreateAction(base::Time::Now(), "brewster");
  Record(activity_db, action);
  ASSERT_EQ(0, CountActions(&activity_db->db_, "brewster"));

  // Artificially trigger and then stop the timer.
  activity_db->SetTimerForTesting(0);
  base::MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(1, CountActions(&activity_db->db_, "brewster"));

  activity_db->Close();
}

TEST_F(ActivityDatabaseTest, BatchModeFlush) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityFlush.db");
  base::DeleteFile(db_file, false);

  // Record some actions
  ActivityDatabase* activity_db = OpenDatabase(db_file);
  activity_db->SetBatchModeForTesting(true);
  scoped_refptr<Action> action = CreateAction(base::Time::Now(), "brewster");
  Record(activity_db, action);
  ASSERT_EQ(0, CountActions(&activity_db->db_, "brewster"));

  // Request an immediate database flush.
  activity_db->AdviseFlush(ActivityDatabase::kFlushImmediately);
  ASSERT_EQ(1, CountActions(&activity_db->db_, "brewster"));

  activity_db->Close();
}

// Check that nothing explodes if the DB isn't initialized.
TEST_F(ActivityDatabaseTest, InitFailure) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("ActivityRecord.db");
  base::DeleteFile(db_file, false);

  ActivityDatabaseTestPolicy* delegate = new ActivityDatabaseTestPolicy();
  ActivityDatabase* activity_db = new ActivityDatabase(delegate);
  scoped_refptr<Action> action = new Action(
      "punky", base::Time::Now(), Action::ACTION_API_CALL, "brewster");
  action->mutable_args()->AppendString("woof");
  delegate->Record(activity_db, action);
  activity_db->Close();
}

}  // namespace extensions
