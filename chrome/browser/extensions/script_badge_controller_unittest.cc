// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/script_badge_controller.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;

namespace extensions {
namespace {

class ScriptBadgeControllerTest : public TabContentsTestHarness {
 public:
  ScriptBadgeControllerTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()) {
  }

  virtual void SetUp() {
    // Note that this sets a PageActionController into the
    // extension_tab_helper()->location_bar_controller() field.  Do
    // not use that for testing.
    TabContentsTestHarness::SetUp();

    TestExtensionSystem* extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(
            tab_contents()->profile()));

    // Create an ExtensionService so the ScriptBadgeController can find its
    // extensions.
    CommandLine command_line(CommandLine::NO_PROGRAM);
    extension_service_ = extension_system->CreateExtensionService(
        &command_line, FilePath(), false);

    script_executor_.reset(new ScriptExecutor(web_contents()));
    script_badge_controller_.reset(new ScriptBadgeController(
        tab_contents(), script_executor_.get()));
  }

 protected:
  ExtensionService* extension_service_;
  scoped_ptr<ScriptExecutor> script_executor_;
  scoped_ptr<ScriptBadgeController> script_badge_controller_;

 private:
  content::TestBrowserThread ui_thread_;
};

struct CountingNotificationObserver : public content::NotificationObserver {
  CountingNotificationObserver() : events(0) {}

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    events++;
  }

  int events;
};

TEST_F(ScriptBadgeControllerTest, ExecutionMakesBadgeVisible) {
  content::NotificationRegistrar notification_registrar;

  EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
              testing::ElementsAre());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "Extension with page action")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("permissions", ListBuilder()
                        .Append("tabs"))
                   .Set("page_action", DictionaryBuilder()
                        .Set("default_title", "Hello")))
      .Build();
  extension_service_->AddExtension(extension);

  // Establish a page id.
  NavigateAndCommit(GURL("http://www.google.com"));

  CountingNotificationObserver location_bar_updated;
  notification_registrar.Add(
      &location_bar_updated,
      chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
      content::Source<Profile>(tab_contents()->profile()));

  // Initially, no script badges.
  EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
              testing::ElementsAre());

  script_badge_controller_->OnExecuteScriptFinished(
      extension->id(), true,
      tab_contents()->web_contents()->GetController().GetActiveEntry()->
      GetPageID(),
      "");
  EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
              testing::ElementsAre(extension->script_badge()));
  EXPECT_THAT(location_bar_updated.events, testing::Gt(0));
};

}  // namespace
}  // namespace extensions
