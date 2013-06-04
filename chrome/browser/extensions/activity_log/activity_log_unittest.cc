// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/activity_log/dom_actions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/dom_action_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace {

const char kExtensionId[] = "abc";

}  // namespace

namespace extensions {

class ActivityLogTest : public testing::Test {
 public:
  ActivityLogTest()
      : message_loop_(base::MessageLoop::TYPE_IO),
        ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {}

  virtual void SetUp() OVERRIDE {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    profile_.reset(new TestingProfile());
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogging);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogTesting);
    ActivityLog::RecomputeLoggingIsEnabled();
  }

  virtual void TearDown() OVERRIDE {
    base::RunLoop().RunUntilIdle();
    profile_.reset(NULL);
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::MessageLoop::QuitClosure(),
        base::TimeDelta::FromSeconds(4));   // Don't hang on failure.
    base::RunLoop().RunUntilIdle();
  }

  static void RetrieveActions_LogAndFetchActions(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  static void Arguments_Missing(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    scoped_refptr<Action> last = i->front();
    std::string id(kExtensionId);
    std::string noargs = "ID: " + id + ", CATEGORY: "
      "CALL, API: tabs.testMethod, ARGS: ";
    ASSERT_EQ(noargs, last->PrintForDebug());
  }

  static void Arguments_Present(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    scoped_refptr<Action> last = i->front();
    std::string id(kExtensionId);
    std::string args = "ID: " + id + ", CATEGORY: "
      "CALL, API: extension.connect, ARGS: \"hello\", \"world\"";
    ASSERT_EQ(args, last->PrintForDebug());
  }

 protected:
  scoped_ptr<TestingProfile> profile_;

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

TEST_F(ActivityLogTest, Enabled) {
  ASSERT_TRUE(ActivityLog::IsLogEnabled());
}

TEST_F(ActivityLogTest, Construct) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_.get());
  scoped_ptr<ListValue> args(new ListValue());
  ASSERT_TRUE(ActivityLog::IsLogEnabled());
  activity_log->LogAPIAction(
      kExtensionId, std::string("tabs.testMethod"), args.get(), std::string());
}

TEST_F(ActivityLogTest, LogAndFetchActions) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_.get());
  scoped_ptr<ListValue> args(new ListValue());
  ASSERT_TRUE(ActivityLog::IsLogEnabled());

  // Write some API calls
  activity_log->LogAPIAction(
      kExtensionId, std::string("tabs.testMethod"), args.get(), std::string());
  activity_log->LogDOMAction(kExtensionId,
                             GURL("http://www.google.com"),
                             string16(),
                             std::string("document.write"),
                             args.get(),
                             DomActionType::METHOD,
                             std::string("extra"));
  activity_log->GetActions(
      kExtensionId,
      0,
      base::Bind(ActivityLogTest::RetrieveActions_LogAndFetchActions));
}

TEST_F(ActivityLogTest, LogWithoutArguments) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_.get());
  activity_log->SetArgumentLoggingForTesting(false);
  ASSERT_TRUE(ActivityLog::IsLogEnabled());

  scoped_ptr<ListValue> args(new ListValue());
  args->Set(0, new base::StringValue("hello"));
  args->Set(1, new base::StringValue("world"));
  activity_log->LogAPIAction(
      kExtensionId, std::string("tabs.testMethod"), args.get(), std::string());
  activity_log->GetActions(
      kExtensionId, 0, base::Bind(ActivityLogTest::Arguments_Missing));
}

TEST_F(ActivityLogTest, LogWithArguments) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_.get());
  ASSERT_TRUE(ActivityLog::IsLogEnabled());

  scoped_ptr<ListValue> args(new ListValue());
  args->Set(0, new base::StringValue("hello"));
  args->Set(1, new base::StringValue("world"));
  activity_log->LogAPIAction(kExtensionId,
                             std::string("extension.connect"),
                             args.get(),
                             std::string());
  activity_log->GetActions(
      kExtensionId, 0, base::Bind(ActivityLogTest::Arguments_Present));
}

}  // namespace extensions

