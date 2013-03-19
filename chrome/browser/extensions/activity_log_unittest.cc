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
#include "chrome/common/chrome_switches.h"
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
        db_thread_(BrowserThread::DB, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {}

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    CommandLine command_line(CommandLine::NO_PROGRAM);
    profile_ =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile_))->CreateExtensionService(
            &command_line, base::FilePath(), false);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityUI);
    ActivityLog::RecomputeLoggingIsEnabled();
  }

  virtual ~ActivityLogTest() {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

  static void RetrieveActions_LogAndFetchActions(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

 protected:
  ExtensionService* extension_service_;
  Profile* profile_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread file_thread_;
};

TEST_F(ActivityLogTest, Enabled) {
  ASSERT_TRUE(ActivityLog::IsLogEnabled());
}

TEST_F(ActivityLogTest, Construct) {
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
  ASSERT_TRUE(ActivityLog::IsLogEnabled());
  activity_log->LogAPIAction(extension,
                             std::string("tabs.testMethod"),
                             args.get(),
                             "");
}

TEST_F(ActivityLogTest, LogAndFetchActions) {
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
  ASSERT_TRUE(ActivityLog::IsLogEnabled());

  // Write some API calls
  activity_log->LogAPIAction(extension,
                             std::string("tabs.testMethod"),
                             args.get(),
                             "");
  activity_log->LogDOMAction(extension,
                             GURL("http://www.google.com"),
                             string16(),
                             std::string("document.write"),
                             args.get(),
                             std::string("extra"));
  activity_log->GetActions(
      extension->id(),
      0,
      base::Bind(ActivityLogTest::RetrieveActions_LogAndFetchActions));
}

}  // namespace extensions

