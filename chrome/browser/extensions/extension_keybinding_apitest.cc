// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

class KeybindingApiTest : public ExtensionApiTest {
 public:
  KeybindingApiTest() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);
  }
  virtual ~KeybindingApiTest() {}

 protected:
  BrowserActionTestUtil GetBrowserActionsBar() {
    return BrowserActionTestUtil(browser());
  }
};

#if !defined(OS_MACOSX)
// Test the basic functionality of the Keybinding API:
// - That pressing the shortcut keys should perform actions (activate the
//   browser action or send an event).
// - Note: Page action keybindings are tested in PageAction test below.
// - The shortcut keys taken by one extension are not overwritten by the last
//   installed extension.
IN_PROC_BROWSER_TEST_F(KeybindingApiTest, Basic) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;
  const extensions::Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Load this extension, which uses the same keybindings but sets the page
  // to different colors. This is so we can see that it doesn't interfere. We
  // don't test this extension in any other way (it should otherwise be
  // immaterial to this test).
  ASSERT_TRUE(RunExtensionTest("keybinding/conflicting")) << message_;

  // Test that there are two browser actions in the toolbar.
  ASSERT_EQ(2, GetBrowserActionsBar().NumberOfBrowserActions());

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/extensions/test_file.txt"));

  // Activate the shortcut (Ctrl+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));

  // Verify the command worked.
  WebContents* tab = chrome::GetActiveWebContents(browser());
  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->GetRenderViewHost(), L"",
      L"setInterval(function(){"
      L"  if(document.body.bgColor == 'red'){"
      L"    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);

  // Activate the shortcut (Ctrl+Shift+Y).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_Y, true, true, false, false));

  result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->GetRenderViewHost(), L"",
      L"setInterval(function(){"
      L"  if(document.body.bgColor == 'blue'){"
      L"    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(KeybindingApiTest, PageAction) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const extensions::Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  {
    // Load a page, the extension will detect the navigation and request to show
    // the page action icon.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        test_server()->GetURL("files/extensions/test_file.txt"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Make sure it appears and is the right one.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  int tab_id = chrome::GetActiveTabContents(browser())->restore_tab_helper()->
      session_id().id();
  ExtensionAction* action = extension->page_action();
  ASSERT_TRUE(action);
  EXPECT_EQ("Make this page red", action->GetTitle(tab_id));

  // Activate the shortcut (Ctrl+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));

  // Verify the command worked (the page action turns the page red).
  WebContents* tab = chrome::GetActiveWebContents(browser());
  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->GetRenderViewHost(), L"",
      L"setInterval(function(){"
      L"  if(document.body.bgColor == 'red'){"
      L"    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

#endif  // !OS_MACOSX
