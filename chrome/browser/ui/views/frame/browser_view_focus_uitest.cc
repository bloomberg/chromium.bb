// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "url/gurl.h"

const char kSimplePage[] = "/focus/page_with_focus.html";

class BrowserViewFocusTest : public InProcessBrowserTest {
 public:
  bool IsViewFocused(ViewID vid) {
    return ui_test_utils::IsViewFocused(browser(), vid);
  }
};

// Flaky, http://crbug.com/69034.
IN_PROC_BROWSER_TEST_F(BrowserViewFocusTest, DISABLED_BrowsersRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // First we navigate to our test page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  gfx::NativeWindow window = browser()->window()->GetNativeWindow();

  // The focus should be on the Tab contents.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  // Now hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  // Hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // The rest of this test does not make sense on Linux because the behavior
  // of Activate() is not well defined and can vary by window manager.
#if defined(OS_WIN)
  // Open a new browser window.
  Browser* browser2 =
      new Browser(Browser::CreateParams(browser()->profile(),
                                        browser()->host_desktop_type()));
  ASSERT_TRUE(browser2);
  chrome::AddTabAt(browser2, GURL(), -1, true);
  browser2->window()->Show();
  ui_test_utils::NavigateToURL(browser2, url);

  gfx::NativeWindow window2 = browser2->window()->GetNativeWindow();
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForBrowser(browser2);
  ASSERT_TRUE(browser_view2);
  const views::Widget* widget2 =
      views::Widget::GetWidgetForNativeWindow(window2);
  ASSERT_TRUE(widget2);
  const views::FocusManager* focus_manager2 = widget2->GetFocusManager();
  ASSERT_TRUE(focus_manager2);
  EXPECT_EQ(browser_view2->GetTabContentsContainerView(),
            focus_manager2->GetFocusedView());

  // Switch to the 1st browser window, focus should still be on the location
  // bar and the second browser should have nothing focused.
  browser()->window()->Activate();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  EXPECT_EQ(NULL, focus_manager2->GetFocusedView());

  // Switch back to the second browser, focus should still be on the page.
  browser2->window()->Activate();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(widget);
  EXPECT_EQ(NULL, widget->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(browser_view2->GetTabContentsContainerView(),
            focus_manager2->GetFocusedView());

  // Close the 2nd browser to avoid a DCHECK().
  browser_view2->Close();
#endif
}
