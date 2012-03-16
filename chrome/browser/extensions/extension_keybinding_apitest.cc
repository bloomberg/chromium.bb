// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
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

  bool OpenPopup(int index) {
    ResultCatcher catcher;
    ui_test_utils::WindowedNotificationObserver popup_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    GetBrowserActionsBar().Press(index);
    popup_observer.Wait();
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
    return GetBrowserActionsBar().HasPopup();
  }
};

#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(KeybindingApiTest, Basic) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/extensions/test_file.txt"));

  // Simulate the browser action being clicked (Ctrl+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));

  // Verify the command worked.
  WebContents* tab = browser()->GetSelectedWebContents();
  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->GetRenderViewHost(), L"",
      L"setInterval(function(){"
      L"  if(document.body.bgColor == 'red'){"
      L"    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);

  // Simulate the event being sent (Ctrl+Shift+Y).
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
#endif  // !OS_MACOSX
