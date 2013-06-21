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
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/dom_action_types.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
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
 protected:
  ActivityLogTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        saved_cmdline_(CommandLine::NO_PROGRAM) {
#if defined OS_CHROMEOS
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    CommandLine command_line(CommandLine::NO_PROGRAM);
    saved_cmdline_ = *CommandLine::ForCurrentProcess();
    profile_.reset(new TestingProfile());
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogging);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogTesting);
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile_.get()))->CreateExtensionService
            (&command_line, base::FilePath(), false);
    ActivityLog::RecomputeLoggingIsEnabled(false);
  }

  virtual ~ActivityLogTest() {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    base::RunLoop().RunUntilIdle();
    profile_.reset(NULL);
    base::RunLoop().RunUntilIdle();
    // Restore the original command line and undo the affects of SetUp().
    *CommandLine::ForCurrentProcess() = saved_cmdline_;
    ActivityLog::RecomputeLoggingIsEnabled(false);
  }

  static void RetrieveActions_LogAndFetchActions(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  static void RetrieveActions_LogAndFetchPathActions(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    std::string args;
    ASSERT_EQ(1U, i->size());
    scoped_refptr<Action> last = i->front();
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableExtensionActivityLogTesting))
      args = "Injected scripts () onto "
              "http://www.google.com/foo?bar extra";
    else
      args = "Injected scripts () onto "
              "http://www.google.com/foo extra";
    ASSERT_EQ(args, last->PrintForDebug());
  }

  static void Arguments_Missing(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    scoped_refptr<Action> last = i->front();
    std::string id(kExtensionId);
    std::string noargs = "ID: " + id + ", CATEGORY: "
        "call, API: tabs.testMethod, ARGS: ";
    ASSERT_EQ(noargs, last->PrintForDebug());
  }

  static void Arguments_Present(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    scoped_refptr<Action> last = i->front();
    std::string id(kExtensionId);
    std::string args = "ID: " + id + ", CATEGORY: "
        "call, API: extension.connect, ARGS: \"hello\", \"world\"";
    ASSERT_EQ(args, last->PrintForDebug());
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;

  content::TestBrowserThreadBundle thread_bundle_;

  // Used to preserve a copy of the original command line.
  // The test framework will do this itself as well. However, by then,
  // it is too late to call ActivityLog::RecomputeLoggingIsEnabled() in
  // TearDown().
  CommandLine saved_cmdline_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif
};

class RenderViewActivityLogTest : public ChromeRenderViewHostTestHarness {
 protected:
  RenderViewActivityLogTest() : saved_cmdline_(CommandLine::NO_PROGRAM) {}

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined OS_CHROMEOS
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    CommandLine command_line(CommandLine::NO_PROGRAM);
    saved_cmdline_ = *CommandLine::ForCurrentProcess();
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogging);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogTesting);
    ActivityLog::RecomputeLoggingIsEnabled(false);
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile()))->CreateExtensionService(
            &command_line, base::FilePath(), false);
  }

  virtual void TearDown() OVERRIDE {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
    *CommandLine::ForCurrentProcess() = saved_cmdline_;
    ActivityLog::RecomputeLoggingIsEnabled(false);
  }

  static void Arguments_Prerender(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(1U, i->size());
    scoped_refptr<Action> last = i->front();
    std::string args = "Injected scripts (\"script \") "
        "onto http://www.google.com/ (prerender)";
    ASSERT_EQ(args, last->PrintForDebug());
  }

  ExtensionService* extension_service_;
  // Used to preserve a copy of the original command line.
  // The test framework will do this itself as well. However, by then,
  // it is too late to call ActivityLog::RecomputeLoggingIsEnabled() in
  // TearDown().
  CommandLine saved_cmdline_;

 private:
#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif
};

TEST_F(ActivityLogTest, Enabled) {
  ASSERT_TRUE(ActivityLog::IsLogEnabledOnAnyProfile());
}

TEST_F(ActivityLogTest, Construct) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_.get());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  ASSERT_TRUE(activity_log->IsLogEnabled());
  activity_log->LogAPIAction(
      kExtensionId, std::string("tabs.testMethod"), args.get(), std::string());
}

TEST_F(ActivityLogTest, LogAndFetchActions) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_.get());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  ASSERT_TRUE(activity_log->IsLogEnabled());

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

TEST_F(ActivityLogTest, LogAndFetchPathActions) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_.get());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  ASSERT_TRUE(activity_log->IsLogEnabled());

  activity_log->LogDOMAction(kExtensionId,
                             GURL("http://www.google.com/foo?bar"),
                             string16(),
                             std::string("document.write"),
                             args.get(),
                             DomActionType::INSERTED,
                             std::string("extra"));
  activity_log->GetActions(
      kExtensionId,
      0,
      base::Bind(ActivityLogTest::RetrieveActions_LogAndFetchPathActions));
}

TEST_F(ActivityLogTest, LogWithoutArguments) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_.get());
  activity_log->SetArgumentLoggingForTesting(false);
  ASSERT_TRUE(activity_log->IsLogEnabled());
  activity_log->SetDefaultPolicy(ActivityLogPolicy::POLICY_NOARGS);
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Set(0, new base::StringValue("hello"));
  args->Set(1, new base::StringValue("world"));
  activity_log->LogAPIAction(
      kExtensionId, std::string("tabs.testMethod"), args.get(), std::string());
  activity_log->GetActions(
      kExtensionId, 0, base::Bind(ActivityLogTest::Arguments_Missing));
}

TEST_F(ActivityLogTest, LogWithArguments) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile_.get());
  activity_log->SetDefaultPolicy(ActivityLogPolicy::POLICY_FULLSTREAM);
  ASSERT_TRUE(activity_log->IsLogEnabled());

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Set(0, new base::StringValue("hello"));
  args->Set(1, new base::StringValue("world"));
  activity_log->LogAPIAction(kExtensionId,
                             std::string("extension.connect"),
                             args.get(),
                             std::string());
  activity_log->GetActions(
      kExtensionId, 0, base::Bind(ActivityLogTest::Arguments_Present));
}

TEST_F(RenderViewActivityLogTest, LogPrerender) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  activity_log->SetDefaultPolicy(ActivityLogPolicy::POLICY_FULLSTREAM);
  ASSERT_TRUE(activity_log->IsLogEnabled());
  GURL url("http://www.google.com");

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()));

  const gfx::Size kSize(640, 480);
  scoped_ptr<prerender::PrerenderHandle> prerender_handle(
      prerender_manager->AddPrerenderFromLocalPredictor(
          url,
          web_contents()->GetController().GetDefaultSessionStorageNamespace(),
          kSize));

  const std::vector<content::WebContents*> contentses =
      prerender_manager->GetAllPrerenderingContents();
  ASSERT_EQ(1U, contentses.size());
  content::WebContents *contents = contentses[0];
  ASSERT_TRUE(prerender_manager->IsWebContentsPrerendering(contents, NULL));

  TabHelper::ScriptExecutionObserver::ExecutingScriptsMap executing_scripts;
  executing_scripts[extension->id()].insert("script");

  static_cast<TabHelper::ScriptExecutionObserver*>(activity_log)->
      OnScriptsExecuted(contents, executing_scripts, 0, url);

  activity_log->GetActions(
      extension->id(), 0, base::Bind(
          RenderViewActivityLogTest::Arguments_Prerender));

  prerender_manager->CancelAllPrerenders();
}

}  // namespace extensions

