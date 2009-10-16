// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, BrowserAction) {
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("browser_action")) << message_;

  // Test that there is a browser action in the toolbar.
  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
  ASSERT_EQ(1, browser_actions->num_browser_actions());

  // Tell the extension to update the browser action state.
  ResultCatcher catcher;
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  Extension* extension = service->extensions()->at(0);
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  ExtensionActionState* action_state = extension->browser_action_state();
  ASSERT_EQ("Modified", action_state->title());
  ASSERT_EQ("badge", action_state->badge_text());
  ASSERT_EQ(SkColorSetARGB(255, 255, 255, 255),
            action_state->badge_background_color());

  // Simulate the browser action being clicked.
  ui_test_utils::NavigateToURL(browser(),
      GURL("http://localhost:1337/files/extensions/test_file.txt"));

  ExtensionAction* browser_action = service->GetBrowserActions(false)[0];
  int window_id = ExtensionTabUtil::GetWindowId(browser());
  ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
      browser()->profile(), browser_action->extension_id(), browser());

  // Verify the command worked.
  TabContents* tab = browser()->GetSelectedTabContents();
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"setInterval(function(){"
      L"  if(document.body.bgColor == 'red'){"
      L"    window.domAutomationController.send(true)}}, 100)",
      &result);
  ASSERT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DynamicBrowserAction) {
  ASSERT_TRUE(RunExtensionTest("browser_action_no_icon")) << message_;

  // Test that there is a browser action in the toolbar.
  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
  ASSERT_EQ(1, browser_actions->num_browser_actions());

  // Tell the extension to update the browser action state.
  ResultCatcher catcher;
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  Extension* extension = service->extensions()->at(0);
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  ExtensionActionState* action_state = extension->browser_action_state();
  ASSERT_TRUE(action_state->icon());
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, BrowserActionPopup) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("popup")));

  ResultCatcher catcher;
  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();

  // Simulate a click on the browser action and verify the size of the resulting
  // popup.
  browser_actions->TestExecuteBrowserAction(0);
  EXPECT_TRUE(browser_actions->TestGetPopup() != NULL);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  gfx::Rect bounds = browser_actions->TestGetPopup()->view()->bounds();
  EXPECT_EQ(100, bounds.width());
  EXPECT_EQ(100, bounds.height());
  browser_actions->HidePopup();
  EXPECT_TRUE(browser_actions->TestGetPopup() == NULL);

  // Do it again, and verify the new bigger size (the popup grows each time it's
  // opened).
  browser_actions->TestExecuteBrowserAction(0);
  EXPECT_TRUE(browser_actions->TestGetPopup() != NULL);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  bounds = browser_actions->TestGetPopup()->view()->bounds();
  EXPECT_EQ(200, bounds.width());
  EXPECT_EQ(200, bounds.height());
  browser_actions->HidePopup();
  EXPECT_TRUE(browser_actions->TestGetPopup() == NULL);
}
