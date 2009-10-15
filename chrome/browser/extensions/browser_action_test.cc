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

#if defined(OS_LINUX)
#include "chrome/browser/gtk/view_id_util.h"
#endif

static void CheckButtonCount(Browser* browser, const int expected_count) {
#if defined(OS_WIN)
  BrowserActionsContainer* browser_actions =
      browser->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
  int num_buttons = browser_actions->num_browser_actions();
#elif defined(OS_LINUX)
  GtkWidget* widget = ViewIDUtil::GetWidget(
      GTK_WIDGET(browser->window()->GetNativeHandle()),
      VIEW_ID_BROWSER_ACTION_TOOLBAR);
  ASSERT_TRUE(widget);
  GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
  int num_buttons = g_list_length(children);
  g_list_free(children);
#endif

  EXPECT_EQ(expected_count, num_buttons);
}

static void TestAction(Browser* browser) {
  // Navigate to a page we have permission to modify.
  ui_test_utils::NavigateToURL(browser,
      GURL("http://localhost:1337/files/extensions/test_file.txt"));

  // Send the command.  Note that this only works for non-popup actions, so
  // we specify |false|.
  ExtensionsService* service = browser->profile()->GetExtensionsService();
  browser->ExecuteCommand(service->GetBrowserActions(false)[0]->command_id());

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

// Crashes frequently on Linux. See http://crbug.com/24802.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, DISABLED_BrowserAction) {
  StartHTTPServer();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("samples")
                    .AppendASCII("make_page_red")));

  // Test that there is a browser action in the toolbar.
  CheckButtonCount(browser(), 1);

  TestAction(browser());
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, BrowserActionNoIcon) {
  StartHTTPServer();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("samples")
                    .AppendASCII("make_page_red_no_icon")));

  // Test that there is a *not* a browser action in the toolbar.
  CheckButtonCount(browser(), 0);

  TestAction(browser());
}
