// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/ui_test_utils.h"

static const char kTestIdentifier1[] = "http_www.google.com_0";

static const char kTestIdentifierExtension[] =
  "chrome-extension_behllobkkfkfnphdnhnkndlbkcpglgmj_0";

class BrowsingDataDatabaseHelperTest : public InProcessBrowserTest {
 public:
  virtual void CreateDatabases() {
    webkit_database::DatabaseTracker* db_tracker =
        testing_profile_.GetDatabaseTracker();
    string16 db_name = ASCIIToUTF16("db");
    string16 description = ASCIIToUTF16("db_description");
    int64 size;
    int64 available;
    string16 identifier1(UTF8ToUTF16(kTestIdentifier1));
    db_tracker->DatabaseOpened(identifier1, db_name, description, 1, &size,
                               &available);
    db_tracker->DatabaseClosed(identifier1, db_name);
    FilePath db_path1 = db_tracker->GetFullDBFilePath(identifier1, db_name);
    file_util::CreateDirectory(db_path1.DirName());
    ASSERT_EQ(0, file_util::WriteFile(db_path1, NULL, 0));
    string16 identifierExtension(UTF8ToUTF16(kTestIdentifierExtension));
    db_tracker->DatabaseOpened(identifierExtension, db_name, description, 1,
                               &size, &available);
    db_tracker->DatabaseClosed(identifierExtension, db_name);
    FilePath db_path2 =
        db_tracker->GetFullDBFilePath(identifierExtension, db_name);
    file_util::CreateDirectory(db_path2.DirName());
    ASSERT_EQ(0, file_util::WriteFile(db_path2, NULL, 0));
    std::vector<webkit_database::OriginInfo> origins;
    db_tracker->GetAllOriginsInfo(&origins);
    ASSERT_EQ(2U, origins.size());
  }

 protected:
  TestingProfile testing_profile_;
};

// Called back by BrowsingDataDatabaseHelper on the UI thread once the database
// information has been retrieved.
class StopTestOnCallback {
 public:
  explicit StopTestOnCallback(
      BrowsingDataDatabaseHelper* database_helper)
      : database_helper_(database_helper) {
    DCHECK(database_helper_);
  }

  void Callback(const std::vector<BrowsingDataDatabaseHelper::DatabaseInfo>&
                database_info_list) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    ASSERT_EQ(1UL, database_info_list.size());
    EXPECT_EQ(std::string(kTestIdentifier1),
              database_info_list.at(0).origin_identifier);
    MessageLoop::current()->Quit();
  }

 private:
  BrowsingDataDatabaseHelper* database_helper_;
};

IN_PROC_BROWSER_TEST_F(BrowsingDataDatabaseHelperTest, FetchData) {
  CreateDatabases();
  scoped_refptr<BrowsingDataDatabaseHelper> database_helper(
      new BrowsingDataDatabaseHelper(&testing_profile_));
  StopTestOnCallback stop_test_on_callback(database_helper);
  database_helper->StartFetching(
      NewCallback(&stop_test_on_callback, &StopTestOnCallback::Callback));
  // Blocks until StopTestOnCallback::Callback is notified.
  ui_test_utils::RunMessageLoop();
}
