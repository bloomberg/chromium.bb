// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

class BrowserActionApiTest : public ExtensionApiTest {
 public:
  BrowserActionApiTest() {}
  virtual ~BrowserActionApiTest() {}

 protected:
  BrowserActionTestUtil GetBrowserActionsBar() {
    return BrowserActionTestUtil(browser());
  }

  bool OpenPopup(int index) {
    ResultCatcher catcher;
    GetBrowserActionsBar().Press(index);
    ui_test_utils::WaitForNotification(
        NotificationType::EXTENSION_POPUP_VIEW_READY);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
    return GetBrowserActionsBar().HasPopup();
  }
};

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, Basic) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("browser_action/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  // Tell the extension to update the browser action state.
  ResultCatcher catcher;
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
      test_server()->GetURL("files/extensions/test_file.txt"));

  ExtensionService* service = browser()->profile()->GetExtensionService();
  service->browser_event_router()->BrowserActionExecuted(
      browser()->profile(), action->extension_id(), browser());

  // Verify the command worked.
  TabContents* tab = browser()->GetSelectedTabContents();
  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"setInterval(function(){"
      L"  if(document.body.bgColor == 'red'){"
      L"    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, DynamicBrowserAction) {
  ASSERT_TRUE(RunExtensionTest("browser_action/no_icon")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar and that it has no icon.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());
  EXPECT_FALSE(GetBrowserActionsBar().HasIcon(0));

  // Tell the extension to update the icon using setIcon({imageData:...}).
  ResultCatcher catcher;
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
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

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

// http://code.google.com/p/chromium/issues/detail?id=70829
// Only mac is okay.
#if !defined(OS_MACOSX)
#define MAYBE_BrowserActionPopup DISABLED_BrowserActionPopup
#else
#define MAYBE_BrowserActionPopup BrowserActionPopup
#endif
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, MAYBE_BrowserActionPopup) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));
  BrowserActionTestUtil actions_bar = GetBrowserActionsBar();
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // The extension's popup's size grows by |growFactor| each click.
  const int growFactor = 500;
  gfx::Size minSize = BrowserActionTestUtil::GetMinPopupSize();
  gfx::Size middleSize = gfx::Size(growFactor, growFactor);
  gfx::Size maxSize = BrowserActionTestUtil::GetMaxPopupSize();

  // Ensure that two clicks will exceed the maximum allowed size.
  ASSERT_GT(minSize.height() + growFactor * 2, maxSize.height());
  ASSERT_GT(minSize.width() + growFactor * 2, maxSize.width());

  // Simulate a click on the browser action and verify the size of the resulting
  // popup.  The first one tries to be 0x0, so it should be the min values.
  ASSERT_TRUE(OpenPopup(0));
  EXPECT_EQ(minSize, actions_bar.GetPopupBounds().size());
  EXPECT_TRUE(actions_bar.HidePopup());

  ASSERT_TRUE(OpenPopup(0));
  EXPECT_EQ(middleSize, actions_bar.GetPopupBounds().size());
  EXPECT_TRUE(actions_bar.HidePopup());

  // One more time, but this time it should be constrained by the max values.
  ASSERT_TRUE(OpenPopup(0));
  EXPECT_EQ(maxSize, actions_bar.GetPopupBounds().size());
  EXPECT_TRUE(actions_bar.HidePopup());
}

// Test that calling chrome.browserAction.setPopup() can enable and change
// a popup.
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, BrowserActionAddPopup) {
  ASSERT_TRUE(RunExtensionTest("browser_action/add_popup")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  int tab_id = ExtensionTabUtil::GetTabId(browser()->GetSelectedTabContents());

  ExtensionAction* browser_action = extension->browser_action();
  ASSERT_TRUE(browser_action)
      << "Browser action test extension should have a browser action.";

  ASSERT_FALSE(browser_action->HasPopup(tab_id));
  ASSERT_FALSE(browser_action->HasPopup(ExtensionAction::kDefaultTabId));

  // Simulate a click on the browser action icon.  The onClicked handler
  // will add a popup.
  {
    ResultCatcher catcher;
    GetBrowserActionsBar().Press(0);
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // The call to setPopup in background.html set a tab id, so the
  // current tab's setting should have changed, but the default setting
  // should not have changed.
  ASSERT_TRUE(browser_action->HasPopup(tab_id))
      << "Clicking on the browser action should have caused a popup to "
      << "be added.";
  ASSERT_FALSE(browser_action->HasPopup(ExtensionAction::kDefaultTabId))
      << "Clicking on the browser action should not have set a default "
      << "popup.";

  ASSERT_STREQ("/a_popup.html",
               browser_action->GetPopupUrl(tab_id).path().c_str());

  // Now change the popup from a_popup.html to another_popup.html by loading
  // a page which removes the popup using chrome.browserAction.setPopup().
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        GURL(extension->GetResourceURL("change_popup.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // The call to setPopup in change_popup.html did not use a tab id,
  // so the default setting should have changed as well as the current tab.
  ASSERT_TRUE(browser_action->HasPopup(tab_id));
  ASSERT_TRUE(browser_action->HasPopup(ExtensionAction::kDefaultTabId));
  ASSERT_STREQ("/another_popup.html",
               browser_action->GetPopupUrl(tab_id).path().c_str());
}

// Test that calling chrome.browserAction.setPopup() can remove a popup.
IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, BrowserActionRemovePopup) {
  // Load the extension, which has a browser action with a default popup.
  ASSERT_TRUE(RunExtensionTest("browser_action/remove_popup")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  int tab_id = ExtensionTabUtil::GetTabId(browser()->GetSelectedTabContents());

  ExtensionAction* browser_action = extension->browser_action();
  ASSERT_TRUE(browser_action)
      << "Browser action test extension should have a browser action.";

  ASSERT_TRUE(browser_action->HasPopup(tab_id))
      << "Expect a browser action popup before the test removes it.";
  ASSERT_TRUE(browser_action->HasPopup(ExtensionAction::kDefaultTabId))
      << "Expect a browser action popup is the default for all tabs.";

  // Load a page which removes the popup using chrome.browserAction.setPopup().
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(),
        GURL(extension->GetResourceURL("remove_popup.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  ASSERT_FALSE(browser_action->HasPopup(tab_id))
      << "Browser action popup should have been removed.";
  ASSERT_TRUE(browser_action->HasPopup(ExtensionAction::kDefaultTabId))
      << "Browser action popup default should not be changed by setting "
      << "a specific tab id.";
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, IncognitoBasic) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(RunExtensionTest("browser_action/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, GetBrowserActionsBar().NumberOfBrowserActions());

  // Open an incognito window and test that the browser action isn't there by
  // default.
  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser = Browser::Create(incognito_profile);

  ASSERT_EQ(0,
            BrowserActionTestUtil(incognito_browser).NumberOfBrowserActions());

  // Now enable the extension in incognito mode, and test that the browser
  // action shows up. Note that we don't update the existing window at the
  // moment, so we just create a new one.
  browser()->profile()->GetExtensionService()->extension_prefs()->
      SetIsIncognitoEnabled(extension->id(), true);

  incognito_browser->CloseWindow();
  incognito_browser = Browser::Create(incognito_profile);
  ASSERT_EQ(1,
            BrowserActionTestUtil(incognito_browser).NumberOfBrowserActions());

  // TODO(mpcomplete): simulate a click and have it do the right thing in
  // incognito.
}

IN_PROC_BROWSER_TEST_F(BrowserActionApiTest, IncognitoDragging) {
  ExtensionService* service = browser()->profile()->GetExtensionService();

  // The tooltips for each respective browser action.
  const char kTooltipA[] = "Make this page red";
  const char kTooltipB[] = "grow";
  const char kTooltipC[] = "Test setPopup()";

  const size_t size_before = service->extensions()->size();

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/basics")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/add_popup")));

  // Test that there are 3 browser actions in the toolbar.
  ASSERT_EQ(size_before + 3, service->extensions()->size());
  ASSERT_EQ(3, GetBrowserActionsBar().NumberOfBrowserActions());

  // Now enable 2 of the extensions in incognito mode, and test that the browser
  // actions show up.
  service->extension_prefs()->SetIsIncognitoEnabled(
      service->extensions()->at(size_before)->id(), true);
  service->extension_prefs()->SetIsIncognitoEnabled(
      service->extensions()->at(size_before + 2)->id(), true);

  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser = Browser::Create(incognito_profile);
  BrowserActionTestUtil incognito_bar(incognito_browser);

  // Navigate just to have a tab in this window, otherwise wonky things happen.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
                                     GURL(chrome::kChromeUIExtensionsURL));

  ASSERT_EQ(2, incognito_bar.NumberOfBrowserActions());

  // Ensure that the browser actions are in the right order (ABC).
  EXPECT_EQ(kTooltipA, GetBrowserActionsBar().GetTooltip(0));
  EXPECT_EQ(kTooltipB, GetBrowserActionsBar().GetTooltip(1));
  EXPECT_EQ(kTooltipC, GetBrowserActionsBar().GetTooltip(2));

  EXPECT_EQ(kTooltipA, incognito_bar.GetTooltip(0));
  EXPECT_EQ(kTooltipC, incognito_bar.GetTooltip(1));

  // Now rearrange them and ensure that they are rearranged correctly in both
  // regular and incognito mode.

  // ABC -> CAB
  service->toolbar_model()->MoveBrowserAction(
      service->extensions()->at(size_before + 2), 0);

  EXPECT_EQ(kTooltipC, GetBrowserActionsBar().GetTooltip(0));
  EXPECT_EQ(kTooltipA, GetBrowserActionsBar().GetTooltip(1));
  EXPECT_EQ(kTooltipB, GetBrowserActionsBar().GetTooltip(2));

  EXPECT_EQ(kTooltipC, incognito_bar.GetTooltip(0));
  EXPECT_EQ(kTooltipA, incognito_bar.GetTooltip(1));

  // CAB -> CBA
  service->toolbar_model()->MoveBrowserAction(
      service->extensions()->at(size_before + 1), 1);

  EXPECT_EQ(kTooltipC, GetBrowserActionsBar().GetTooltip(0));
  EXPECT_EQ(kTooltipB, GetBrowserActionsBar().GetTooltip(1));
  EXPECT_EQ(kTooltipA, GetBrowserActionsBar().GetTooltip(2));

  EXPECT_EQ(kTooltipC, incognito_bar.GetTooltip(0));
  EXPECT_EQ(kTooltipA, incognito_bar.GetTooltip(1));
}
