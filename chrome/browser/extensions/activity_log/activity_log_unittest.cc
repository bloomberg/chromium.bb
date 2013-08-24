// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
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

class ActivityLogTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined OS_CHROMEOS
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    CommandLine command_line(CommandLine::NO_PROGRAM);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogging);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogTesting);
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile()))->CreateExtensionService
            (&command_line, base::FilePath(), false);
    base::RunLoop().RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    base::RunLoop().RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  static void RetrieveActions_LogAndFetchActions(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  void SetPolicy(bool log_arguments) {
    ActivityLog* activity_log = ActivityLog::GetInstance(profile());
    if (log_arguments)
      activity_log->SetDefaultPolicy(ActivityLogPolicy::POLICY_FULLSTREAM);
    else
      activity_log->SetDefaultPolicy(ActivityLogPolicy::POLICY_COUNTS);
  }

  static void Arguments_Prerender(
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    ASSERT_EQ(1U, i->size());
    scoped_refptr<Action> last = i->front();
    std::string args =
        "ID=odlameecjipmbmbejkplpemijjgpljce CATEGORY=content_script API= "
        "ARGS=[\"script\"] PAGE_URL=http://www.google.com/ "
        "OTHER={\"prerender\":true}";
    ASSERT_EQ(args, last->PrintForDebug());
  }

  ExtensionService* extension_service_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif
};

TEST_F(ActivityLogTest, Construct) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  ASSERT_TRUE(activity_log->IsLogEnabled());
}

TEST_F(ActivityLogTest, LogAndFetchActions) {
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  ASSERT_TRUE(activity_log->IsLogEnabled());

  // Write some API calls
  scoped_refptr<Action> action = new Action(kExtensionId,
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "tabs.testMethod");
  activity_log->LogAction(action);
  action = new Action(kExtensionId,
                      base::Time::Now(),
                      Action::ACTION_DOM_ACCESS,
                      "document.write");
  action->set_page_url(GURL("http://www.google.com"));
  activity_log->LogAction(action);

  activity_log->GetActions(
      kExtensionId,
      0,
      base::Bind(ActivityLogTest::RetrieveActions_LogAndFetchActions));
}

TEST_F(ActivityLogTest, LogPrerender) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());
  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
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
      extension->id(), 0, base::Bind(ActivityLogTest::Arguments_Prerender));

  prerender_manager->CancelAllPrerenders();
}

}  // namespace extensions
