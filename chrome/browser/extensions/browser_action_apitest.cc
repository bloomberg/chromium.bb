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
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/ui_test_utils.h"

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/browser/views/toolbar_view.h"
#elif defined(OS_LINUX)
#include "chrome/browser/gtk/view_id_util.h"
#endif

class BrowserActionTest : public ExtensionApiTest {
 public:
  BrowserActionTest() { }
  virtual ~BrowserActionTest() { }

  int NumberOfBrowserActions() {
    int rv = -1;

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
    BrowserActionsContainer* browser_actions =
        browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
        browser_actions();
    if (browser_actions)
      rv = browser_actions->num_browser_actions();
#elif defined(OS_LINUX)
    GtkWidget* toolbar = ViewIDUtil::GetWidget(
        GTK_WIDGET(browser()->window()->GetNativeHandle()),
        VIEW_ID_BROWSER_ACTION_TOOLBAR);

    if (toolbar) {
      GList* children = gtk_container_get_children(GTK_CONTAINER(toolbar));
      rv = g_list_length(children);
      g_list_free(children);
    }
#endif

    EXPECT_NE(-1, rv);
    return rv;
  }

  bool IsIconNull(int index) {
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
    BrowserActionsContainer* browser_actions =
        browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
        browser_actions();
    // We can't ASSERT_TRUE in non-void functions.
    if (browser_actions) {
      return browser_actions->GetBrowserActionViewAt(index)->button()->icon().
          empty();
    } else {
      EXPECT_TRUE(false);
    }
#elif defined(OS_LINUX)
    GtkWidget* button = GetButton(index);
    if (button)
      return gtk_button_get_image(GTK_BUTTON(button)) == NULL;
    else
      EXPECT_TRUE(false);
#endif

    return false;
  }

  void ExecuteBrowserAction(int index) {
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
    BrowserActionsContainer* browser_actions =
        browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
        browser_actions();
    ASSERT_TRUE(browser_actions);
    browser_actions->TestExecuteBrowserAction(index);
#elif defined(OS_LINUX) || defined(TOOLKIT_VIEWS)
    GtkWidget* button = GetButton(index);
    ASSERT_TRUE(button);
    gtk_button_clicked(GTK_BUTTON(button));
#endif
  }

  std::string GetTooltip(int index) {
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
    BrowserActionsContainer* browser_actions =
        browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
        browser_actions();
    if (browser_actions) {
      std::wstring text;
      EXPECT_TRUE(browser_actions->GetBrowserActionViewAt(0)->button()->
            GetTooltipText(0, 0, &text));
      return WideToUTF8(text);
    }
#elif defined(OS_LINUX)
    GtkWidget* button = GetButton(index);
    if (button) {
      gchar* text = gtk_widget_get_tooltip_text(button);
      std::string rv = std::string(text);
      g_free(text);
      return rv;
    }
#endif
    EXPECT_TRUE(false);
    return std::string();
  }

 private:
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  GtkWidget* GetButton(int index) {
    GtkWidget* rv = NULL;
    GtkWidget* toolbar = ViewIDUtil::GetWidget(
        GTK_WIDGET(browser()->window()->GetNativeHandle()),
        VIEW_ID_BROWSER_ACTION_TOOLBAR);

    if (toolbar) {
      GList* children = gtk_container_get_children(GTK_CONTAINER(toolbar));
      rv = static_cast<GtkWidget*>(g_list_nth(children, index)->data);
      g_list_free(children);
    }

    return rv;
  }
#endif
};

IN_PROC_BROWSER_TEST_F(BrowserActionTest, Basic) {
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("browser_action")) << message_;

  // Test that there is a browser action in the toolbar.
  ASSERT_EQ(1, NumberOfBrowserActions());

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

IN_PROC_BROWSER_TEST_F(BrowserActionTest, DynamicBrowserAction) {
  ASSERT_TRUE(RunExtensionTest("browser_action_no_icon")) << message_;

  // Test that there is a browser action in the toolbar and that it has no icon.
  ASSERT_EQ(1, NumberOfBrowserActions());
  EXPECT_TRUE(IsIconNull(0));

  // Tell the extension to update the icon using setIcon({imageData:...}).
  ResultCatcher catcher;
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  Extension* extension = service->extensions()->at(0);
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  EXPECT_FALSE(IsIconNull(0));

  // Tell the extension to update using setIcon({path:...});
  ui_test_utils::NavigateToURL(browser(),
      GURL(extension->GetResourceURL("update2.html")));
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that we received the changes.
  EXPECT_FALSE(IsIconNull(0));

  // TODO(aa): Would be nice here to actually compare that the pixels change.
}

IN_PROC_BROWSER_TEST_F(BrowserActionTest, TabSpecificBrowserActionState) {
  ASSERT_TRUE(RunExtensionTest("browser_action_tab_specific_state")) <<
      message_;

  // Test that there is a browser action in the toolbar and that it has an icon.
  ASSERT_EQ(1, NumberOfBrowserActions());
  EXPECT_FALSE(IsIconNull(0));

  // Execute the action, its title should change.
  ResultCatcher catcher;
  ExecuteBrowserAction(0);
  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_EQ("Showing icon 2", GetTooltip(0));

  // Open a new tab, the title should go back.
  browser()->NewTab();
  EXPECT_EQ("hi!", GetTooltip(0));

  // Go back to first tab, changed title should reappear.
  browser()->SelectTabContentsAt(0, true);
  EXPECT_EQ("Showing icon 2", GetTooltip(0));

  // Reload that tab, default title should come back.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_EQ("hi!", GetTooltip(0));
}

// TODO(estade): port to Linux.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(BrowserActionTest, BrowserActionPopup) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("popup")));

  ResultCatcher catcher;

  // This value is in api_test/popup/popup.html.
  const int growFactor = 500;
  ASSERT_GT(ExtensionPopup::kMinHeight + growFactor * 2,
            ExtensionPopup::kMaxHeight);
  ASSERT_GT(ExtensionPopup::kMinWidth + growFactor * 2,
            ExtensionPopup::kMaxWidth);

  // Our initial expected size.
  int width = ExtensionPopup::kMinWidth;
  int height = ExtensionPopup::kMinHeight;

  BrowserActionsContainer* browser_actions =
      browser()->window()->GetBrowserWindowTesting()->GetToolbarView()->
      browser_actions();
  ASSERT_TRUE(browser_actions);
  // Simulate a click on the browser action and verify the size of the resulting
  // popup.  The first one tries to be 0x0, so it should be the min values.
  ExecuteBrowserAction(0);
  EXPECT_TRUE(browser_actions->TestGetPopup() != NULL);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  gfx::Rect bounds = browser_actions->TestGetPopup()->view()->bounds();
  EXPECT_EQ(width, bounds.width());
  EXPECT_EQ(height, bounds.height());
  browser_actions->HidePopup();
  EXPECT_TRUE(browser_actions->TestGetPopup() == NULL);

  // Do it again, and verify the new bigger size (the popup grows each time it's
  // opened).
  width = growFactor;
  height = growFactor;
  browser_actions->TestExecuteBrowserAction(0);
  EXPECT_TRUE(browser_actions->TestGetPopup() != NULL);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  bounds = browser_actions->TestGetPopup()->view()->bounds();
  EXPECT_EQ(width, bounds.width());
  EXPECT_EQ(height, bounds.height());
  browser_actions->HidePopup();
  EXPECT_TRUE(browser_actions->TestGetPopup() == NULL);

  // One more time, but this time it should be constrained by the max values.
  width = ExtensionPopup::kMaxWidth;
  height = ExtensionPopup::kMaxHeight;
  browser_actions->TestExecuteBrowserAction(0);
  EXPECT_TRUE(browser_actions->TestGetPopup() != NULL);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  bounds = browser_actions->TestGetPopup()->view()->bounds();
  EXPECT_EQ(width, bounds.width());
  EXPECT_EQ(height, bounds.height());
  browser_actions->HidePopup();
  EXPECT_TRUE(browser_actions->TestGetPopup() == NULL);
}
#endif
