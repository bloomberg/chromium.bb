// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/script_badge_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
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

class ScriptBadgeControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ScriptBadgeControllerTest()
      : feature_override_(FeatureSwitch::script_badges(), true),
        ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()),
        current_channel_(chrome::VersionInfo::CHANNEL_DEV) {}

  virtual void SetUp() OVERRIDE {
    // Note that this sets a PageActionController into the
    // extensions::TabHelper's location_bar_controller field.  Do
    // not use that for testing.
    ChromeRenderViewHostTestHarness::SetUp();

    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    TestExtensionSystem* extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile));

    // Create an ExtensionService so the ScriptBadgeController can find its
    // extensions.
    CommandLine command_line(CommandLine::NO_PROGRAM);
    extension_service_ = extension_system->CreateExtensionService(
        &command_line, base::FilePath(), false);

    TabHelper::CreateForWebContents(web_contents());
    script_badge_controller_ = static_cast<ScriptBadgeController*>(
        TabHelper::FromWebContents(web_contents())->location_bar_controller());
  }

 protected:
  // Creates a test extension and adds it to |extension_service_|.
  scoped_refptr<const Extension> AddTestExtension() {
    scoped_refptr<const Extension> extension = ExtensionBuilder()
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
    return extension;
  }

  ExtensionAction* GetScriptBadge(const Extension& extension) {
    return ExtensionActionManager::Get(profile())->GetScriptBadge(extension);
  }

  ExtensionService* extension_service_;
  ScriptBadgeController* script_badge_controller_;

 private:
  FeatureSwitch::ScopedOverride feature_override_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  Feature::ScopedCurrentChannel current_channel_;
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

  scoped_refptr<const Extension> extension = AddTestExtension();

  // Establish a page id.
  NavigateAndCommit(GURL("http://www.google.com"));

  CountingNotificationObserver location_bar_updated;
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  notification_registrar.Add(
      &location_bar_updated,
      chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
      content::Source<Profile>(profile));

  // Initially, no script badges.
  EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
              testing::ElementsAre());

  TabHelper::ScriptExecutionObserver::ExecutingScriptsMap id_map;
  id_map[extension->id()] = std::set<std::string>();
  script_badge_controller_->OnScriptsExecuted(
      web_contents(),
      id_map,
      web_contents()->GetController().GetActiveEntry()->GetPageID(),
      GURL(""));
  EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
              testing::ElementsAre(GetScriptBadge(*extension)));
  EXPECT_THAT(location_bar_updated.events, testing::Gt(0));
};

TEST_F(ScriptBadgeControllerTest, FragmentNavigation) {
  scoped_refptr<const Extension> extension = AddTestExtension();
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  // Establish a page id.
  NavigateAndCommit(GURL("http://www.google.com"));

  // Run script. Should be a notification and a script badge.
  {
    content::NotificationRegistrar notification_registrar;
    CountingNotificationObserver location_bar_updated;
    notification_registrar.Add(
        &location_bar_updated,
        chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
        content::Source<Profile>(profile));

    TabHelper::ScriptExecutionObserver::ExecutingScriptsMap id_map;
    id_map[extension->id()] = std::set<std::string>();
    script_badge_controller_->OnScriptsExecuted(
        web_contents(),
        id_map,
        web_contents()->GetController().GetActiveEntry()->GetPageID(),
        GURL(""));

    EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
                testing::ElementsAre(GetScriptBadge(*extension)));
    EXPECT_EQ(1, location_bar_updated.events);
  }

  // Navigate to a hash fragment. Shouldn't change.
  {
    content::NotificationRegistrar notification_registrar;
    CountingNotificationObserver location_bar_updated;
    notification_registrar.Add(
        &location_bar_updated,
        chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
        content::Source<Profile>(profile));

    NavigateAndCommit(GURL("http://www.google.com#hash"));

    EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
              testing::ElementsAre(GetScriptBadge(*extension)));
    EXPECT_EQ(0, location_bar_updated.events);
  }

  // Refreshing the page should reset the badges.
  {
    content::NotificationRegistrar notification_registrar;
    CountingNotificationObserver location_bar_updated;
    notification_registrar.Add(
        &location_bar_updated,
        chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
        content::Source<Profile>(profile));

    Reload();

    EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
                testing::ElementsAre());
  }
}

TEST_F(ScriptBadgeControllerTest, GetAttentionMakesBadgeVisible) {
  content::NotificationRegistrar notification_registrar;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "Extension")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("permissions", ListBuilder()
                        .Append("tabs")))
      .Build();
  extension_service_->AddExtension(extension);

  // Establish a page id.
  NavigateAndCommit(GURL("http://www.google.com"));

  CountingNotificationObserver initial_badge_display;
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  notification_registrar.Add(
      &initial_badge_display,
      chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
      content::Source<Profile>(profile));

  // Initially, no script badges.
  EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
              testing::ElementsAre());

  // Getting attention the first time should display the badge.
  script_badge_controller_->GetAttentionFor(extension->id());

  EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
              testing::ElementsAre(GetScriptBadge(*extension)));
  EXPECT_THAT(initial_badge_display.events, testing::Gt(0));

  CountingNotificationObserver subsequent_get_attention_call;
  notification_registrar.Add(
      &subsequent_get_attention_call,
      chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
      content::Source<Profile>(profile));

  // Getting attention a second time should have no effect.
  script_badge_controller_->GetAttentionFor(extension->id());

  EXPECT_THAT(script_badge_controller_->GetCurrentActions(),
              testing::ElementsAre(GetScriptBadge(*extension)));
  EXPECT_EQ(0, subsequent_get_attention_call.events);
};

}  // namespace
}  // namespace extensions
