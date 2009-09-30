// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/ui_test_utils.h"

static void TestAction(Browser* browser) {
  // Navigate to a page we have permission to modify.
  ui_test_utils::NavigateToURL(browser,
      GURL("http://localhost:1337/files/extensions/test_file.txt"));

  // Send the command.
  ExtensionsService* service = browser->profile()->GetExtensionsService();
  browser->ExecuteCommand(service->GetBrowserActions()[0]->command_id());

  // Verify the command worked.
  TabContents* tab = browser->GetSelectedTabContents();
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"setInterval(function(){"
      L"  if(document.body.bgColor == 'red'){"
      L"    window.domAutomationController.send(true)}}, 100)",
      &result);
  ASSERT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, BrowserAction) {
  StartHTTPServer();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("samples")
                    .AppendASCII("make_page_red")));

  // Test that there is a browser action in the toolbar.
  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
  ASSERT_EQ(1, browser_actions->num_browser_actions());

  TestAction(browser());
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, BrowserActionNoIcon) {
  StartHTTPServer();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("samples")
                    .AppendASCII("make_page_red_no_icon")));

  // Test that there is a *not* a browser action in the toolbar.
  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
  ASSERT_EQ(0, browser_actions->num_browser_actions());

  TestAction(browser());
}
