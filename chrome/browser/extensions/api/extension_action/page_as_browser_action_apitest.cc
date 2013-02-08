// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"

// These are a mash-up of the tests from from page_actions_apitest.cc and
// browser_actions_apitest.cc.

namespace extensions {
namespace {

class PageAsBrowserActionApiTest : public ExtensionApiTest {
 public:
  PageAsBrowserActionApiTest() {}
  virtual ~PageAsBrowserActionApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kScriptBadges, "1");
  }

 protected:
  BrowserActionTestUtil GetBrowserActionsBar() {
    return BrowserActionTestUtil(browser());
  }

  ExtensionActionManager* extension_action_manager() {
    return ExtensionActionManager::Get(browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(PageAsBrowserActionApiTest, Basic) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("page_action/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // The extension declares a page action, but it should have gotten a browser
  // action instead.
  ASSERT_TRUE(extension_action_manager()->GetBrowserAction(*extension));
  ASSERT_FALSE(extension_action_manager()->GetPageAction(*extension));

  // With the "action box" there won't be browser actions unless they're pinned.
  ExtensionPrefs* prefs = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service()->extension_prefs();
  prefs->SetBrowserActionVisibility(extension, true);

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  {
    // Tell the extension to update the page action state.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        GURL(extension->GetResourceURL("update.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Test that we received the changes.
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());
  ExtensionAction* action =
      extension_action_manager()->GetBrowserAction(*extension);
  ASSERT_TRUE(action);
  EXPECT_EQ("Modified", action->GetTitle(tab_id));

  {
    // Simulate the page action being clicked.
    ResultCatcher catcher;
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    service->toolbar_model()->ExecuteBrowserAction(extension, browser(), NULL);
    EXPECT_TRUE(catcher.GetNextResult());
  }

  {
    // Tell the extension to update the page action state again.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        GURL(extension->GetResourceURL("update2.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // We should not be creating icons asynchronously, so we don't need an
  // observer.
  ExtensionActionIconFactory icon_factory(
      profile(), extension, action, NULL);

  // Test that we received the changes.
  EXPECT_FALSE(icon_factory.GetIcon(tab_id).IsEmpty());
}

// Test that calling chrome.pageAction.setPopup() can enable a popup.
IN_PROC_BROWSER_TEST_F(PageAsBrowserActionApiTest, AddPopup) {
  // Load the extension, which has no default popup.
  ASSERT_TRUE(RunExtensionTest("page_action/add_popup")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  ExtensionAction* page_action =
      extension_action_manager()->GetBrowserAction(*extension);
  ASSERT_TRUE(page_action)
      << "Page action test extension should have a page action.";

  ASSERT_FALSE(page_action->HasPopup(tab_id));

  // Simulate the page action being clicked.  The resulting event should
  // install a page action popup.
  {
    ResultCatcher catcher;
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    service->toolbar_model()->ExecuteBrowserAction(extension, browser(), NULL);
    ASSERT_TRUE(catcher.GetNextResult());
  }

  ASSERT_TRUE(page_action->HasPopup(tab_id))
      << "Clicking on the page action should have caused a popup to be added.";

  ASSERT_STREQ("/a_popup.html",
               page_action->GetPopupUrl(tab_id).path().c_str());

  // Now change the popup from a_popup.html to a_second_popup.html .
  // Load a page which removes the popup using chrome.pageAction.setPopup().
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        GURL(extension->GetResourceURL("change_popup.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  ASSERT_TRUE(page_action->HasPopup(tab_id));
  ASSERT_STREQ("/another_popup.html",
               page_action->GetPopupUrl(tab_id).path().c_str());
}

// Test that calling chrome.pageAction.setPopup() can remove a popup.
IN_PROC_BROWSER_TEST_F(PageAsBrowserActionApiTest, RemovePopup) {
  // Load the extension, which has a page action with a default popup.
  ASSERT_TRUE(RunExtensionTest("page_action/remove_popup")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  ExtensionAction* page_action =
      extension_action_manager()->GetBrowserAction(*extension);
  ASSERT_TRUE(page_action)
      << "Page action test extension should have a page action.";

  ASSERT_TRUE(page_action->HasPopup(tab_id))
      << "Expect a page action popup before the test removes it.";

  // Load a page which removes the popup using chrome.pageAction.setPopup().
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        GURL(extension->GetResourceURL("remove_popup.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  ASSERT_FALSE(page_action->HasPopup(tab_id))
      << "Page action popup should have been removed.";
}

IN_PROC_BROWSER_TEST_F(PageAsBrowserActionApiTest, Getters) {
  ASSERT_TRUE(RunExtensionTest("page_action/getters")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());
}

}
}  // namespace extensions
