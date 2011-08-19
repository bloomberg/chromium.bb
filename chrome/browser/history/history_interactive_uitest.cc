// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History interactive UI tests

#include "chrome/browser/history/history_uitest.h"

namespace {

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const FilePath::CharType* const kHistoryDir = FILE_PATH_LITERAL("History");

}  // namespace

TEST_F(HistoryTester, ConsiderRedirectAfterGestureAsUserInitiated) {
  // Test the history length for the following page transition.
  //
  // -open-> Page 11 -slow_redirect-> Page 12.
  //
  // If redirect occurs after a user gesture, e.g., mouse click, the
  // redirect is more likely to be user-initiated rather than automatic.
  // Therefore, Page 11 should be in the history in addition to Page 12.

  const FilePath test_case(
      FILE_PATH_LITERAL("history_length_test_page_11.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case);
  NavigateToURL(url);
  WaitForFinish("History_Length_Test_11", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());

  // Simulate click. This only works for Windows.
  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window = browser->GetWindow();
  ASSERT_TRUE(window.get());
  gfx::Rect tab_view_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &tab_view_bounds,
                                    true));
  ASSERT_TRUE(window->SimulateOSClick(tab_view_bounds.CenterPoint(),
                                      ui::EF_LEFT_BUTTON_DOWN));

  NavigateToURL(GURL("javascript:redirectToPage12()"));
  WaitForFinish("History_Length_Test_12", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}
