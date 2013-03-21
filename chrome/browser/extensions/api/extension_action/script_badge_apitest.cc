// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_event_router.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {
namespace {

class ScriptBadgeApiTest : public ExtensionApiTest {
 public:
  ScriptBadgeApiTest() : trunk_(chrome::VersionInfo::CHANNEL_UNKNOWN) {}

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kScriptBadges, "1");
  }

 private:
  extensions::Feature::ScopedCurrentChannel trunk_;
};

IN_PROC_BROWSER_TEST_F(ScriptBadgeApiTest, Basics) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("script_badge/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;
  ExtensionAction* script_badge =
      ExtensionActionManager::Get(browser()->profile())->
      GetScriptBadge(*extension);
  ASSERT_TRUE(script_badge);
  const extensions::LocationBarController* location_bar_controller =
      extensions::TabHelper::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())->
              location_bar_controller();

  const int tab_id = SessionID::IdForTab(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(GURL(extension->GetResourceURL("default_popup.html")),
            script_badge->GetPopupUrl(tab_id));

  EXPECT_FALSE(script_badge->GetIsVisible(tab_id));
  EXPECT_THAT(location_bar_controller->GetCurrentActions(),
              testing::ElementsAre());

  {
    ResultCatcher catcher;
    // Tell the extension to update the script badge state.
    ui_test_utils::NavigateToURL(
        browser(), GURL(extension->GetResourceURL("update.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Test that we received the changes.
  EXPECT_EQ(GURL(extension->GetResourceURL("set_popup.html")),
            script_badge->GetPopupUrl(tab_id));

  {
    // Simulate the script badge being clicked.
    ResultCatcher catcher;
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    service->browser_event_router()->ScriptBadgeExecuted(
        browser()->profile(), *script_badge, tab_id);
    EXPECT_TRUE(catcher.GetNextResult());
  }

  {
    ResultCatcher catcher;
    // Visit a non-extension page so the extension can run a content script and
    // cause the script badge to be animated in.
    ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(""));
    ASSERT_TRUE(catcher.GetNextResult());
  }
  EXPECT_TRUE(script_badge->GetIsVisible(tab_id));
  EXPECT_THAT(location_bar_controller->GetCurrentActions(),
              testing::ElementsAre(script_badge));
}

}  // namespace
}  // namespace extensions
