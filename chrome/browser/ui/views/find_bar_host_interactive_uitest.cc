// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/keyboard_codes.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/test/test_server.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"

#if defined(OS_WIN)
#include <windows.h>
#include <Psapi.h>
#endif

namespace {

// The delay waited after sending an OS simulated event.
static const int kActionDelayMs = 500;
static const char kSimplePage[] = "files/find_in_page/simple.html";

class FindInPageTest : public InProcessBrowserTest {
 public:
  FindInPageTest() {
    set_show_window(true);
    FindBarHost::disable_animations_during_testing_ = true;
  }

  string16 GetFindBarText() {
    FindBarTesting* find_bar =
        browser()->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetFindText();
  }
};

void Checkpoint(const char* message, const base::TimeTicks& start_time) {
  LOG(INFO) << message << " : "
    << (base::TimeTicks::Now() - start_time).InMilliseconds()
    << " ms" << std::flush;
}

// Test to make sure Chrome is in the foreground as we start testing. This is
// required for tests that synthesize input to the Chrome window.
bool ChromeInForeground() {
#if defined(OS_WIN)
  HWND window = ::GetForegroundWindow();
  std::wstring caption;
  std::wstring filename;
  int len = ::GetWindowTextLength(window) + 1;
  ::GetWindowText(window, WriteInto(&caption, len), len);
  bool chrome_window_in_foreground =
      (caption == L"about:blank - Google Chrome") ||
      (caption == L"about:blank - Chromium");
  if (!chrome_window_in_foreground) {
    DWORD process_id;
    int thread_id = ::GetWindowThreadProcessId(window, &process_id);

    base::ProcessHandle process;
    if (base::OpenProcessHandleWithAccess(process_id,
                                          PROCESS_QUERY_LIMITED_INFORMATION,
                                          &process)) {
      len = MAX_PATH;
      if (!GetProcessImageFileName(process, WriteInto(&filename, len), len)) {
        int error = GetLastError();
        filename = std::wstring(L"Unable to read filename for process id '" +
                                base::IntToString16(process_id) +
                                L"' (error ") +
                                base::IntToString16(error) + L")";
      }
      base::CloseProcessHandle(process);
    }
  }
  EXPECT_TRUE(chrome_window_in_foreground)
      << "Chrome must be in the foreground when running interactive tests\n"
      << "Process in foreground: " << filename.c_str() << "\n"
      << "Window: " << window << "\n"
      << "Caption: " << caption.c_str();
  return chrome_window_in_foreground;
#else
  // Windows only at the moment.
  return true;
#endif
}

}  // namespace

IN_PROC_BROWSER_TEST_F(FindInPageTest, CrashEscHandlers) {
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page (tab A).
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->Find();

  // Open another tab (tab B).
  browser()->AddSelectedTabWithURL(url, PageTransition::TYPED);

  browser()->Find();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Select tab A.
  browser()->SelectTabContentsAt(0, true);

  // Close tab B.
  browser()->CloseTabContents(browser()->GetTabContentsAt(1));

  // Click on the location bar so that Find box loses focus.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                     VIEW_ID_LOCATION_BAR));
#if defined(TOOLKIT_VIEWS) || defined(OS_WIN)
  // Check the location bar is focused.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));
#endif

  // This used to crash until bug 1303709 was fixed.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), app::VKEY_ESCAPE, false, false, false, false));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, FocusRestore) {
  ASSERT_TRUE(test_server()->Start());

  GURL url = test_server()->GetURL("title1.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Focus the location bar, open and close the find-in-page, focus should
  // return to the location bar.
  browser()->FocusLocationBar();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));
  // Ensure the creation of the find bar controller.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelection);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));

  // Focus the location bar, find something on the page, close the find box,
  // focus should go to the page.
  browser()->FocusLocationBar();
  browser()->Find();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ui_test_utils::FindInPage(browser()->GetSelectedTabContents(),
                            ASCIIToUTF16("a"), true, false, NULL);
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelection);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Focus the location bar, open and close the find box, focus should return to
  // the location bar (same as before, just checking that http://crbug.com/23599
  // is fixed).
  browser()->FocusLocationBar();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelection);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, FocusRestoreOnTabSwitch) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  Checkpoint("Starting test server", start_time);

  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page (tab A).
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  Checkpoint("Calling Find", start_time);

  browser()->Find();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  Checkpoint("GetFindBarTesting", start_time);

  FindBarTesting* find_bar =
      browser()->GetFindBarController()->find_bar()->GetFindBarTesting();

  Checkpoint("Search for 'a'", start_time);

  // Search for 'a'.
  ui_test_utils::FindInPage(browser()->GetSelectedTabContents(),
                            ASCIIToUTF16("a"), true, false, NULL);
  EXPECT_TRUE(ASCIIToUTF16("a") == find_bar->GetFindSelectedText());

  Checkpoint("Open tab B", start_time);

  // Open another tab (tab B).
  browser()->AddSelectedTabWithURL(url, PageTransition::TYPED);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));

  Checkpoint("Open find", start_time);

  // Make sure Find box is open.
  browser()->Find();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  Checkpoint("Search for 'b'", start_time);

  // Search for 'b'.
  ui_test_utils::FindInPage(browser()->GetSelectedTabContents(),
                            ASCIIToUTF16("b"), true, false, NULL);
  EXPECT_TRUE(ASCIIToUTF16("b") == find_bar->GetFindSelectedText());

  Checkpoint("Focus location bar", start_time);

  // Set focus away from the Find bar (to the Location bar).
  browser()->FocusLocationBar();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));

  Checkpoint("Select tab A", start_time);

  // Select tab A. Find bar should get focus.
  browser()->SelectTabContentsAt(0, true);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_TRUE(ASCIIToUTF16("a") == find_bar->GetFindSelectedText());

  Checkpoint("Select tab B", start_time);

  // Select tab B. Location bar should get focus.
  browser()->SelectTabContentsAt(1, true);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_LOCATION_BAR));

  Checkpoint("Test done", start_time);
}

// This tests that whenever you clear values from the Find box and close it that
// it respects that and doesn't show you the last search, as reported in bug:
// http://crbug.com/40121.
IN_PROC_BROWSER_TEST_F(FindInPageTest, PrepopulateRespectBlank) {
#if defined(OS_MACOSX)
  // FindInPage on Mac doesn't use prepopulated values. Search there is global.
  return;
#endif

  base::TimeTicks start_time = base::TimeTicks::Now();
  Checkpoint("Starting test server", start_time);

  ASSERT_TRUE(test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  EXPECT_TRUE(ChromeInForeground());

  // First we navigate to any page.
  Checkpoint("Navigating", start_time);
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Show the Find bar.
  Checkpoint("Showing Find window", start_time);
  browser()->GetFindBarController()->Show();

  // Search for "a".
  Checkpoint("Search for 'a'", start_time);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), app::VKEY_A, false, false, false, false));

  // We should find "a" here.
  Checkpoint("GetFindBarText", start_time);
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarText());

  // Delete "a".
  Checkpoint("Delete 'a'", start_time);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), app::VKEY_BACK, false, false, false, false));

  // Validate we have cleared the text.
  Checkpoint("Validate clear", start_time);
  EXPECT_EQ(string16(), GetFindBarText());

  // Close the Find box.
  Checkpoint("Close find", start_time);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), app::VKEY_ESCAPE, false, false, false, false));

  // Show the Find bar.
  Checkpoint("Showing Find window", start_time);
  browser()->GetFindBarController()->Show();

  // After the Find box has been reopened, it should not have been prepopulated
  // with "a" again.
  Checkpoint("GetFindBarText", start_time);
  EXPECT_EQ(string16(), GetFindBarText());

  // Close the Find box.
  Checkpoint("Press Esc", start_time);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), app::VKEY_ESCAPE, false, false, false, false));

  // Press F3 to trigger FindNext.
  Checkpoint("Press F3", start_time);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), app::VKEY_F3, false, false, false, false));

  // After the Find box has been reopened, it should still have no prepopulate
  // value.
  Checkpoint("GetFindBarText", start_time);
  EXPECT_EQ(string16(), GetFindBarText());

  Checkpoint("Test completed", start_time);
}
