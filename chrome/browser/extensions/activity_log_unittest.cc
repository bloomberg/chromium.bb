// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/extensions/activity_log.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ActivityLogTest : public ChromeRenderViewHostTestHarness {
 public:
  ActivityLogTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        db_thread_(BrowserThread::DB),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {}

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    CommandLine command_line(CommandLine::NO_PROGRAM);
    profile_ =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile_))->CreateExtensionService(
            &command_line, FilePath(), false);
    db_thread_.Start();
  }

  ~ActivityLogTest() {
    base::WaitableEvent done(false, false);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
    db_thread_.Stop();
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

 protected:
  ExtensionService* extension_service_;
  Profile* profile_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread file_thread_;
};

TEST_F(ActivityLogTest, ConstructAndLog) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_);
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension);
  scoped_ptr<ListValue> args(new ListValue());
  for (int i = 0; i < 30; i++) {
    // Run this a bunch of times and hope that if something goes wrong with
    // threading, 30 times is enough to cause it to fail.
    activity_log->LogAPIAction(extension,
                               std::string("tabs.testMethod"),
                               args.get(),
                               "");
  }
  // Need to ensure the writes were completed.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(3));
  FilePath db_file = profile_->GetPath().Append(
      chrome::kExtensionActivityLogFilename);
  sql::Connection db;
  ASSERT_TRUE(db.Open(db_file));
  std::string sql_str = "SELECT * FROM " +
      std::string(APIAction::kTableName);
  sql::Statement statement(db.GetUniqueStatement(sql_str.c_str()));
  ASSERT_TRUE(statement.Step());
  ASSERT_EQ("UNKNOWN_ACTION", statement.ColumnString(2));
  ASSERT_EQ("TABS", statement.ColumnString(3));
  ASSERT_EQ("tabs.testMethod()", statement.ColumnString(4));
}

}  // namespace extensions

