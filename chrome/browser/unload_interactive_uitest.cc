// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unload_uitest.h"

const char CLOSE_TAB_WHEN_OTHER_TAB_HAS_LISTENER[] =
    "<html><head><title>only_one_unload</title></head>"
    "<body onclick=\"window.open('data:text/html,"
    "<html><head><title>popup</title></head></body>')\" "
    "onbeforeunload='return;'>"
    "</body></html>";

#if defined(OS_MACOSX)
// http://crbug.com/45162
#define MAYBE_BrowserCloseTabWhenOtherTabHasListener \
    DISABLED_BrowserCloseTabWhenOtherTabHasListener
#elif defined(OS_WIN)
// http://crbug.com/45281
#define MAYBE_BrowserCloseTabWhenOtherTabHasListener \
    DISABLED_BrowserCloseTabWhenOtherTabHasListener
#elif defined(OS_CHROMEOS)
// http://crbug.com/86769
#define MAYBE_BrowserCloseTabWhenOtherTabHasListener \
    FLAKY_BrowserCloseTabWhenOtherTabHasListener
#else
#define MAYBE_BrowserCloseTabWhenOtherTabHasListener \
    BrowserCloseTabWhenOtherTabHasListener
#endif

// Tests that if there's a renderer process with two tabs, one of which has an
// unload handler, and the other doesn't, the tab that doesn't have an unload
// handler can be closed.
TEST_F(UnloadTest, MAYBE_BrowserCloseTabWhenOtherTabHasListener) {
  NavigateToDataURL(CLOSE_TAB_WHEN_OTHER_TAB_HAS_LISTENER, L"only_one_unload");

  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window = browser->GetWindow();
  ASSERT_TRUE(window.get());

  gfx::Rect tab_view_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER,
              &tab_view_bounds, true));
  // Simulate a click to force user_gesture to true; if we don't, the resulting
  // popup will be constrained, which isn't what we want to test.
  ASSERT_TRUE(window->SimulateOSClick(tab_view_bounds.CenterPoint(),
                                      ui::EF_LEFT_BUTTON_DOWN));
  ASSERT_TRUE(browser->WaitForTabCountToBecome(2));

  CheckTitle(L"popup");
  scoped_refptr<TabProxy> popup_tab(browser->GetActiveTab());
  ASSERT_TRUE(popup_tab.get());
  EXPECT_TRUE(popup_tab->Close(true));

  ASSERT_TRUE(browser->WaitForTabCountToBecome(1));
  scoped_refptr<TabProxy> main_tab(browser->GetActiveTab());
  ASSERT_TRUE(main_tab.get());
  std::wstring main_title;
  EXPECT_TRUE(main_tab->GetTabTitle(&main_title));
  EXPECT_EQ(std::wstring(L"only_one_unload"), main_title);
}
