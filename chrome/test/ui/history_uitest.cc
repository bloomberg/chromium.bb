// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History UI tests

#include "base/file_path.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui_test_utils.h"
#include "gfx/rect.h"
#include "views/event.h"

namespace {

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const FilePath::CharType* kHistoryDir = FILE_PATH_LITERAL("History");

}  // namespace

class HistoryTester : public UITest {
 protected:
  HistoryTester() : UITest() { }

  virtual void SetUp() {
    show_window_ = true;
    UITest::SetUp();
  }
};

TEST_F(HistoryTester, VerifyHistoryLength1) {
  // Test the history length for the following page transitions.
  //   -open-> Page 1.

  const FilePath test_case_1(
      FILE_PATH_LITERAL("history_length_test_page_1.html"));
  GURL url_1 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_1);
  NavigateToURL(url_1);
  WaitForFinish("History_Length_Test_1", "1", url_1, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

TEST_F(HistoryTester, VerifyHistoryLength2) {
  // Test the history length for the following page transitions.
  //   -open-> Page 2 -redirect-> Page 3.

  const FilePath test_case_2(
      FILE_PATH_LITERAL("history_length_test_page_2.html"));
  GURL url_2 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_2);
  NavigateToURL(url_2);
  WaitForFinish("History_Length_Test_2", "1", url_2, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

TEST_F(HistoryTester, VerifyHistoryLength3) {
  // Test the history length for the following page transitions.
  // -open-> Page 1 -> open Page 2 -redirect Page 3. open Page 4
  // -navigate_backward-> Page 3 -navigate_backward->Page 1
  // -navigate_forward-> Page 3 -navigate_forward-> Page 4
  const FilePath test_case_1(
      FILE_PATH_LITERAL("history_length_test_page_1.html"));
  GURL url_1 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_1);
  NavigateToURL(url_1);
  WaitForFinish("History_Length_Test_1", "1", url_1, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());

  const FilePath test_case_2(
      FILE_PATH_LITERAL("history_length_test_page_2.html"));
  GURL url_2 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_2);
  NavigateToURL(url_2);
  WaitForFinish("History_Length_Test_2", "1", url_2, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());

  const FilePath test_case_3(
      FILE_PATH_LITERAL("history_length_test_page_4.html"));
  GURL url_3 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_3);
  NavigateToURL(url_3);
  WaitForFinish("History_Length_Test_3", "1", url_3, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

#if defined(OS_WIN) || defined(OS_LINUX)
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
  ASSERT_TRUE(
      window->SimulateOSClick(tab_view_bounds.CenterPoint(),
                              views::Event::EF_LEFT_BUTTON_DOWN));

  NavigateToURL(GURL("javascript:redirectToPage12()"));
  WaitForFinish("History_Length_Test_12", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}
#endif  // defined(OS_WIN) || defined(OS_LINUX)

TEST_F(HistoryTester, ConsiderSlowRedirectAsUserInitiated) {
  // Test the history length for the following page transition.
  //
  // -open-> Page 21 -redirect-> Page 22.
  //
  // If redirect occurs more than 5 seconds later after the page is loaded,
  // the redirect is likely to be user-initiated.
  // Therefore, Page 21 should be in the history in addition to Page 22.

  const FilePath test_case(
      FILE_PATH_LITERAL("history_length_test_page_21.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case);
  NavigateToURL(url);
  WaitForFinish("History_Length_Test_21", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}
