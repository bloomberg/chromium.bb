// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/ui_test_utils.h"

static const int kTimeoutMs = 60 * 1000;  // 1 minute

class BrowserActionApiTest : public ExtensionApiTest {
 public:
  BrowserActionApiTest() {}
  virtual ~BrowserActionApiTest() {}

 protected:
  BrowserActionTestUtil GetBrowserActionsBar() {
    return BrowserActionTestUtil(browser());
  }

  gfx::Rect OpenPopup(int index) {
    {
      NotificationRegistrar registrar;
      registrar.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                    NotificationService::AllSources());
      GetBrowserActionsBar().Press(index);
      // If the popup is already showing then we needn't wait for the
      // notification before proceeding.
      if (!GetBrowserActionsBar().HasPopup()) {
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE, new MessageLoop::QuitTask, kTimeoutMs);
      }
      ui_test_utils::RunMessageLoop();
    }
    EXPECT_TRUE(GetBrowserActionsBar().HasPopup());
    return GetBrowserActionsBar().GetPopupBounds();
  }
};

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, Basic) {
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("browser_action/basics")) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  // Tell the extension to update the browser action state.
  ResultCatcher catcher;
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  Extension* extension = service->extensions()->at(0);
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  ExtensionAction* action = extension->browser_action();
  ASSERT_EQ("Modified", action->GetTitle(ExtensionAction::kDefaultTabId));
  ASSERT_EQ("badge", action->GetBadgeText(ExtensionAction::kDefaultTabId));
  ASSERT_EQ(SkColorSetARGB(255, 255, 255, 255),
            action->GetBadgeBackgroundColor(ExtensionAction::kDefaultTabId));

  // Simulate the browser action being clicked.
  ui_test_utils::NavigateToURL(browser(),
      GURL("http://localhost:1337/files/extensions/test_file.txt"));

  ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
      browser()->profile(), action->extension_id(), browser());

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

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, DynamicBrowserAction) {
  ASSERT_TRUE(RunExtensionTest("browser_action/no_icon")) << message_;

  // Test that there is a browser action in the toolbar and that it has no icon.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());
  EXPECT_FALSE(GetBrowserActionsBar().HasIcon(0));

  // Tell the extension to update the icon using setIcon({imageData:...}).
  ResultCatcher catcher;
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  Extension* extension = service->extensions()->at(0);
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  EXPECT_TRUE(GetBrowserActionsBar().HasIcon(0));

  // Tell the extension to update using setIcon({path:...});
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update2.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  EXPECT_TRUE(GetBrowserActionsBar().HasIcon(0));

  // TODO(aa): Would be nice here to actually compare that the pixels change.
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, TabSpecificBrowserActionState) {
  ASSERT_TRUE(RunExtensionTest("browser_action/tab_specific_state")) <<
      message_;

  // Test that there is a browser action in the toolbar and that it has an icon.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());
  EXPECT_TRUE(GetBrowserActionsBar().HasIcon(0));

  // Execute the action, its title should change.
  ResultCatcher catcher;
  GetBrowserActionsBar().Press(0);
  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_EQ("Showing icon 2", GetBrowserActionsBar().GetTooltip(0));

  // Open a new tab, the title should go back.
  browser()->NewTab();
  EXPECT_EQ("hi!", GetBrowserActionsBar().GetTooltip(0));

  // Go back to first tab, changed title should reappear.
  browser()->SelectTabContentsAt(0, true);
  EXPECT_EQ("Showing icon 2", GetBrowserActionsBar().GetTooltip(0));

  // Reload that tab, default title should come back.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_EQ("hi!", GetBrowserActionsBar().GetTooltip(0));
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, FLAKY_BrowserActionPopup) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));

  // The extension's popup's size grows by |growFactor| each click.
  const int growFactor = 500;
  gfx::Size minSize = BrowserActionTestUtil::GetMinPopupSize();
  gfx::Size maxSize = BrowserActionTestUtil::GetMaxPopupSize();

  // Ensure that two clicks will exceed the maximum allowed size.
  ASSERT_GT(minSize.height() + growFactor * 2, maxSize.height());
  ASSERT_GT(minSize.width() + growFactor * 2, maxSize.width());

  // Simulate a click on the browser action and verify the size of the resulting
  // popup.  The first one tries to be 0x0, so it should be the min values.
  gfx::Rect bounds = OpenPopup(0);
  EXPECT_EQ(minSize, bounds.size());
  EXPECT_TRUE(GetBrowserActionsBar().HidePopup());

  bounds = OpenPopup(0);
  EXPECT_EQ(gfx::Size(growFactor, growFactor), bounds.size());
  EXPECT_TRUE(GetBrowserActionsBar().HidePopup());

  // One more time, but this time it should be constrained by the max values.
  bounds = OpenPopup(0);
  EXPECT_EQ(maxSize, bounds.size());
  EXPECT_TRUE(GetBrowserActionsBar().HidePopup());
}
