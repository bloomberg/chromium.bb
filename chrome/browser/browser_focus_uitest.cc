// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/test/test_server.h"

#if defined(TOOLKIT_VIEWS) || defined(OS_WIN)
#include "views/focus/focus_manager.h"
#include "views/view.h"
#include "views/window/window.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/view_id_util.h"
#endif

#if defined(OS_WIN)
#include <Psapi.h>
#include <windows.h>
#endif

#if defined(OS_LINUX)
#define MAYBE_FocusTraversal FocusTraversal
#define MAYBE_FocusTraversalOnInterstitial FocusTraversalOnInterstitial
// TODO(jcampan): http://crbug.com/23683
#define MAYBE_TabsRememberFocusFindInPage FAILS_TabsRememberFocusFindInPage
#elif defined(OS_MACOSX)
// TODO(suzhe): http://crbug.com/60973 (following two tests)
#define MAYBE_FocusTraversal DISABLED_FocusTraversal
#define MAYBE_FocusTraversalOnInterstitial DISABLED_FocusTraversalOnInterstitial
// TODO(suzhe): http://crbug.com/49737
#define MAYBE_TabsRememberFocusFindInPage FAILS_TabsRememberFocusFindInPage
#elif defined(OS_WIN)
// Disabled, http://crbug.com/62543.
#define MAYBE_FocusTraversal DISABLED_FocusTraversal
// Disabled, http://crbug.com/62544.
#define MAYBE_FocusTraversalOnInterstitial DISABLED_FocusTraversalOnInterstitial
// Flaky, http://crbug.com/62537.
#define MAYBE_TabsRememberFocusFindInPage FLAKY_TabsRememberFocusFindInPage
#endif

namespace {

// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;

const char kSimplePage[] = "files/focus/page_with_focus.html";
const char kStealFocusPage[] = "files/focus/page_steals_focus.html";
const char kTypicalPage[] = "files/focus/typical_page.html";
const char kTypicalPageName[] = "typical_page.html";

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
      EndsWith(caption, L" - Google Chrome", true) ||
      EndsWith(caption, L" - Chromium", true);
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

class BrowserFocusTest : public InProcessBrowserTest {
 public:
  BrowserFocusTest() {
    set_show_window(true);
    EnableDOMAutomation();
  }

  bool IsViewFocused(ViewID vid) {
    return ui_test_utils::IsViewFocused(browser(), vid);
  }

  void ClickOnView(ViewID vid) {
    ui_test_utils::ClickOnView(browser(), vid);
  }
};

class TestInterstitialPage : public InterstitialPage {
 public:
  TestInterstitialPage(TabContents* tab, bool new_navigation, const GURL& url)
      : InterstitialPage(tab, new_navigation, url) {
    FilePath file_path;
    bool r = PathService::Get(chrome::DIR_TEST_DATA, &file_path);
    EXPECT_TRUE(r);
    file_path = file_path.AppendASCII("focus");
    file_path = file_path.AppendASCII(kTypicalPageName);
    r = file_util::ReadFileToString(file_path, &html_contents_);
    EXPECT_TRUE(r);
  }

  virtual std::string GetHTMLContents() {
    return html_contents_;
  }

  // Exposing render_view_host() and tab() to be public; they are declared as
  // protected in the superclass.
  virtual RenderViewHost* render_view_host() {
    return InterstitialPage::render_view_host();
  }

  virtual TabContents* tab() {
    return InterstitialPage::tab();
  }

  bool HasFocus() {
    return render_view_host()->view()->HasFocus();
  }

 protected:
  virtual void FocusedNodeChanged(bool is_editable_node) {
    NotificationService::current()->Notify(
        NotificationType::FOCUS_CHANGED_IN_PAGE,
        Source<TabContents>(tab()),
        Details<const bool>(&is_editable_node));
  }

 private:
  std::string html_contents_;
};

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, ClickingMovesFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#if defined(OS_POSIX)
  // It seems we have to wait a little bit for the widgets to spin up before
  // we can start clicking on them.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(),
                                          kActionDelayMs);
  ui_test_utils::RunMessageLoop();
#endif  // defined(OS_POSIX)

  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

  ClickOnView(VIEW_ID_TAB_CONTAINER);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  ClickOnView(VIEW_ID_LOCATION_BAR);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
}

// Flaky, http://crbug.com/69034.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FLAKY_BrowsersRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page.
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  gfx::NativeWindow window = browser()->window()->GetNativeHandle();

  // The focus should be on the Tab contents.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
  // Now hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ui_test_utils::ShowAndFocusNativeWindow(window);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  browser()->FocusLocationBar();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
  // Hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ui_test_utils::ShowAndFocusNativeWindow(window);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

  // The rest of this test does not make sense on Linux because the behavior
  // of Activate() is not well defined and can vary by window manager.
#if defined(OS_WIN)
  // Open a new browser window.
  Browser* browser2 = Browser::Create(browser()->profile());
  ASSERT_TRUE(browser2);
  browser2->tabstrip_model()->delegate()->AddBlankTab(true);
  browser2->window()->Show();
  ui_test_utils::NavigateToURL(browser2, url);

  HWND hwnd2 = reinterpret_cast<HWND>(browser2->window()->GetNativeHandle());
  BrowserView* browser_view2 =
      BrowserView::GetBrowserViewForNativeWindow(hwnd2);
  ASSERT_TRUE(browser_view2);
  views::FocusManager* focus_manager2 =
      views::FocusManager::GetFocusManagerForNativeView(hwnd2);
  ASSERT_TRUE(focus_manager2);
  EXPECT_EQ(browser_view2->GetTabContentsContainerView(),
            focus_manager2->GetFocusedView());

  // Switch to the 1st browser window, focus should still be on the location
  // bar and the second browser should have nothing focused.
  browser()->window()->Activate();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
  EXPECT_EQ(NULL, focus_manager2->GetFocusedView());

  // Switch back to the second browser, focus should still be on the page.
  browser2->window()->Activate();
  EXPECT_EQ(NULL,
            views::FocusManager::GetFocusManagerForNativeView(
                browser()->window()->GetNativeHandle())->GetFocusedView());
  EXPECT_EQ(browser_view2->GetTabContentsContainerView(),
            focus_manager2->GetFocusedView());

  // Close the 2nd browser to avoid a DCHECK().
  browser_view2->Close();
#endif
}

// Tabs remember focus.
// Disabled, http://crbug.com/62542.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_TabsRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page.
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Create several tabs.
  for (int i = 0; i < 4; ++i)
    browser()->AddSelectedTabWithURL(url, PageTransition::TYPED);

  // Alternate focus for the tab.
  const bool kFocusPage[3][5] = {
    { true, true, true, true, false },
    { false, false, false, false, false },
    { false, true, false, true, false }
  };

  for (int i = 1; i < 3; i++) {
    for (int j = 0; j < 5; j++) {
      // Activate the tab.
      browser()->SelectTabContentsAt(j, true);

      // Activate the location bar or the page.
      if (kFocusPage[i][j]) {
        browser()->GetTabContentsAt(j)->view()->Focus();
      } else {
        browser()->FocusLocationBar();
      }
    }

    // Now come back to the tab and check the right view is focused.
    for (int j = 0; j < 5; j++) {
      // Activate the tab.
      browser()->SelectTabContentsAt(j, true);

      ViewID vid = kFocusPage[i][j] ? VIEW_ID_TAB_CONTAINER_FOCUS_VIEW :
                                      VIEW_ID_LOCATION_BAR;
      ASSERT_TRUE(IsViewFocused(vid));
    }

    browser()->SelectTabContentsAt(0, true);
    // Try the above, but with ctrl+tab. Since tab normally changes focus,
    // this has regressed in the past. Loop through several times to be sure.
    for (int j = 0; j < 15; j++) {
      ViewID vid = kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER_FOCUS_VIEW :
                                          VIEW_ID_LOCATION_BAR;
      ASSERT_TRUE(IsViewFocused(vid));

      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
          browser(), ui::VKEY_TAB, true, false, false, false));
    }

    // As above, but with ctrl+shift+tab.
    browser()->SelectTabContentsAt(4, true);
    for (int j = 14; j >= 0; --j) {
      ViewID vid = kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER_FOCUS_VIEW :
                                          VIEW_ID_LOCATION_BAR;
      ASSERT_TRUE(IsViewFocused(vid));

      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
          browser(), ui::VKEY_TAB, true, true, false, false));
    }
  }
}

// Tabs remember focus with find-in-page box.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_TabsRememberFocusFindInPage) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page.
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->Find();
  ui_test_utils::FindInPage(browser()->GetSelectedTabContents(),
                            ASCIIToUTF16("a"), true, false, NULL);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Focus the location bar.
  browser()->FocusLocationBar();

  // Create a 2nd tab.
  browser()->AddSelectedTabWithURL(url, PageTransition::TYPED);

  // Focus should be on the recently opened tab page.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Select 1st tab, focus should still be on the location-bar.
  // (bug http://crbug.com/23296)
  browser()->SelectTabContentsAt(0, true);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

  // Now open the find box again, switch to another tab and come back, the focus
  // should return to the find box.
  browser()->Find();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->SelectTabContentsAt(1, true);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
  browser()->SelectTabContentsAt(0, true);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

// Background window does not steal focus.
// Flaky, http://crbug.com/62538.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest,
                       FLAKY_BackgroundBrowserDontStealFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // Open a new browser window.
  Browser* browser2 = Browser::Create(browser()->profile());
  ASSERT_TRUE(browser2);
  browser2->tabstrip_model()->delegate()->AddBlankTab(true);
  browser2->window()->Show();

  Browser* focused_browser = NULL;
  Browser* unfocused_browser = NULL;
#if defined(USE_X11)
  // On X11, calling Activate() is not guaranteed to move focus, so we have
  // to figure out which browser does have focus.
  if (browser2->window()->IsActive()) {
    focused_browser = browser2;
    unfocused_browser = browser();
  } else if (browser()->window()->IsActive()) {
    focused_browser = browser();
    unfocused_browser = browser2;
  } else {
    FAIL() << "Could not determine which browser has focus";
  }
#elif defined(OS_WIN)
  focused_browser = browser();
  unfocused_browser = browser2;
#elif defined(OS_MACOSX)
  // On Mac, the newly created window always gets the focus.
  focused_browser = browser2;
  unfocused_browser = browser();
#endif

  GURL steal_focus_url = test_server()->GetURL(kStealFocusPage);
  ui_test_utils::NavigateToURL(unfocused_browser, steal_focus_url);

  // Activate the first browser.
  focused_browser->window()->Activate();

  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      unfocused_browser->GetSelectedTabContents()->render_view_host(), L"",
      L"stealFocus();"));

  // Make sure the first browser is still active.
  EXPECT_TRUE(focused_browser->window()->IsActive());
}

// Page cannot steal focus when focus is on location bar.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, LocationBarLockFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // Open the page that steals focus.
  GURL url = test_server()->GetURL(kStealFocusPage);
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->FocusLocationBar();

  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"stealFocus();"));

  // Make sure the location bar is still focused.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
}

// Focus traversal on a regular page.
// Note that this test relies on a notification from the renderer that the
// focus has changed in the page.  The notification in the renderer may change
// at which point this test would fail (see comment in
// RenderWidget::didFocus()).
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FocusTraversal) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page.
  GURL url = test_server()->GetURL(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->FocusLocationBar();

  const char* kTextElementID = "textEdit";
  const char* kExpElementIDs[] = {
    "",  // Initially no element in the page should be focused
         // (the location bar is focused).
    kTextElementID, "searchButton", "luckyButton", "googleLink", "gmailLink",
    "gmapLink"
  };

  // Test forward focus traversal.
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(StringPrintf("outer loop: %d", i));
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

    // Move the caret to the end, otherwise the next Tab key may not move focus.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_END, false, false, false, false));

    // Now let's press tab to move the focus.
    for (size_t j = 0; j < arraysize(kExpElementIDs); ++j) {
      SCOPED_TRACE(StringPrintf("inner loop %" PRIuS, j));
      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"",
          L"window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(kExpElementIDs[j], actual.c_str());

      if (j < arraysize(kExpElementIDs) - 1) {
        // If the next element is the kTextElementID, we expect to be
        // notified we have switched to an editable node.
        bool is_editable_node =
            (strcmp(kTextElementID, kExpElementIDs[j + 1]) == 0);
        Details<bool> details(&is_editable_node);

        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWaitWithDetails(
            browser(), ui::VKEY_TAB, false, false, false, false,
            NotificationType::FOCUS_CHANGED_IN_PAGE,
            NotificationSource(Source<TabContents>(
                browser()->GetSelectedTabContents())),
            details));
      } else {
        // On the last tab key press, the focus returns to the browser.
        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
            browser(), ui::VKEY_TAB, false, false, false, false,
            NotificationType::FOCUS_RETURNED_TO_BROWSER,
            NotificationSource(Source<Browser>(browser()))));
      }
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    ui_test_utils::RunAllPendingInMessageLoop();
  }

  // Now let's try reverse focus traversal.
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(StringPrintf("outer loop: %d", i));
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

    // Move the caret to the end, otherwise the next Tab key may not move focus.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_END, false, false, false, false));

    // Now let's press shift-tab to move the focus in reverse.
    for (size_t j = 0; j < arraysize(kExpElementIDs); ++j) {
      SCOPED_TRACE(StringPrintf("inner loop: %" PRIuS, j));
      const char* next_element =
          kExpElementIDs[arraysize(kExpElementIDs) - 1 - j];

      if (j < arraysize(kExpElementIDs) - 1) {
        // If the next element is the kTextElementID, we expect to be
        // notified we have switched to an editable node.
        bool is_editable_node = (strcmp(kTextElementID, next_element) == 0);
        Details<bool> details(&is_editable_node);

        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWaitWithDetails(
            browser(), ui::VKEY_TAB, false, true, false, false,
            NotificationType::FOCUS_CHANGED_IN_PAGE,
            NotificationSource(Source<TabContents>(
                browser()->GetSelectedTabContents())),
            details));
      } else {
        // On the last tab key press, the focus returns to the browser.
        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
            browser(), ui::VKEY_TAB, false, true, false, false,
            NotificationType::FOCUS_RETURNED_TO_BROWSER,
            NotificationSource(Source<Browser>(browser()))));
      }

      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"",
          L"window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(next_element, actual.c_str());
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    ui_test_utils::RunAllPendingInMessageLoop();
  }
}

// Focus traversal while an interstitial is showing.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FocusTraversalOnInterstitial) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page.
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Focus should be on the page.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Let's show an interstitial.
  TestInterstitialPage* interstitial_page =
      new TestInterstitialPage(browser()->GetSelectedTabContents(),
                               true, GURL("http://interstitial.com"));
  interstitial_page->Show();
  // Give some time for the interstitial to show.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(),
                                          1000);
  ui_test_utils::RunMessageLoop();

  browser()->FocusLocationBar();

  const char* kExpElementIDs[] = {
    "",  // Initially no element in the page should be focused
         // (the location bar is focused).
    "textEdit", "searchButton", "luckyButton", "googleLink", "gmailLink",
    "gmapLink"
  };

  // Test forward focus traversal.
  for (int i = 0; i < 2; ++i) {
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

    // Move the caret to the end, otherwise the next Tab key may not move focus.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_END, false, false, false, false));

    // Now let's press tab to move the focus.
    for (size_t j = 0; j < 7; ++j) {
      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          interstitial_page->render_view_host(), L"",
          L"window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(kExpElementIDs[j], actual.c_str());

      NotificationType::Type notification_type;
      NotificationSource notification_source =
          NotificationService::AllSources();
      if (j < arraysize(kExpElementIDs) - 1) {
        notification_type = NotificationType::FOCUS_CHANGED_IN_PAGE;
        notification_source = Source<TabContents>(
            interstitial_page->tab());
      } else {
        // On the last tab key press, the focus returns to the browser.
        notification_type = NotificationType::FOCUS_RETURNED_TO_BROWSER;
        notification_source = Source<Browser>(browser());
      }

      ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
          browser(), ui::VKEY_TAB, false, false, false, false,
          notification_type, notification_source));
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    ui_test_utils::RunAllPendingInMessageLoop();
  }

  // Now let's try reverse focus traversal.
  for (int i = 0; i < 2; ++i) {
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

    // Move the caret to the end, otherwise the next Tab key may not move focus.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_END, false, false, false, false));

    // Now let's press shift-tab to move the focus in reverse.
    for (size_t j = 0; j < 7; ++j) {
      NotificationType::Type notification_type;
      NotificationSource notification_source =
          NotificationService::AllSources();
      if (j < arraysize(kExpElementIDs) - 1) {
        notification_type = NotificationType::FOCUS_CHANGED_IN_PAGE;
        notification_source = Source<TabContents>(
            interstitial_page->tab());
      } else {
        // On the last tab key press, the focus returns to the browser.
        notification_type = NotificationType::FOCUS_RETURNED_TO_BROWSER;
        notification_source = Source<Browser>(browser());
      }

      ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
          browser(), ui::VKEY_TAB, false, true, false, false,
          notification_type, notification_source));

      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          interstitial_page->render_view_host(), L"",
          L"window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(kExpElementIDs[6 - j], actual.c_str());
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    ui_test_utils::RunAllPendingInMessageLoop();
  }
}

// Focus stays on page with interstitials.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, InterstitialFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page.
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Page should have focus.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
  EXPECT_TRUE(browser()->GetSelectedTabContents()->render_view_host()->view()->
      HasFocus());

  // Let's show an interstitial.
  TestInterstitialPage* interstitial_page =
      new TestInterstitialPage(browser()->GetSelectedTabContents(),
                               true, GURL("http://interstitial.com"));
  interstitial_page->Show();
  // Give some time for the interstitial to show.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(),
                                          1000);
  ui_test_utils::RunMessageLoop();

  // The interstitial should have focus now.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
  EXPECT_TRUE(interstitial_page->HasFocus());

  // Hide the interstitial.
  interstitial_page->DontProceed();

  // Focus should be back on the original page.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
}

// Make sure Find box can request focus, even when it is already open.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FindFocusTest) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // Open some page (any page that doesn't steal focus).
  GURL url = test_server()->GetURL(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(ChromeInForeground());

#if defined(OS_MACOSX)
  // Press Cmd+F, which will make the Find box open and request focus.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, false, false, true));
#else
  // Press Ctrl+F, which will make the Find box open and request focus.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, false, false, false));
#endif

  // Ideally, we wouldn't sleep here and instead would intercept the
  // RenderViewHostDelegate::HandleKeyboardEvent() callback.  To do that, we
  // could create a RenderViewHostDelegate wrapper and hook-it up by either:
  // - creating a factory used to create the delegate
  // - making the test a private and overwriting the delegate member directly.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, new MessageLoop::QuitTask(), kActionDelayMs);
  ui_test_utils::RunMessageLoop();

  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  browser()->FocusLocationBar();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

  // Now press Ctrl+F again and focus should move to the Find box.
#if defined(OS_MACOSX)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, false, false, true));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, false, false, false));
#endif
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Set focus to the page.
  ClickOnView(VIEW_ID_TAB_CONTAINER);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Now press Ctrl+F again and focus should move to the Find box.
#if defined(OS_MACOSX)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, false, false, true));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, false, false, false));
#endif

  // See remark above on why we wait.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, new MessageLoop::QuitTask(), kActionDelayMs);
  ui_test_utils::RunMessageLoop();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

// Makes sure the focus is in the right location when opening the different
// types of tabs.
// Flaky, http://crbug.com/62539.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FLAKY_TabInitialFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open the history tab, focus should be on the tab contents.
  browser()->ShowHistoryTab();
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::WaitForLoadStop(
      &browser()->GetSelectedTabContents()->controller()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Open the new tab, focus should be on the location bar.
  browser()->NewTab();
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::WaitForLoadStop(
      &browser()->GetSelectedTabContents()->controller()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

  // Open the download tab, focus should be on the tab contents.
  browser()->ShowDownloadsTab();
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::WaitForLoadStop(
      &browser()->GetSelectedTabContents()->controller()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Open about:blank, focus should be on the location bar.
  browser()->AddSelectedTabWithURL(GURL(chrome::kAboutBlankURL),
                                   PageTransition::LINK);
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::WaitForLoadStop(
      &browser()->GetSelectedTabContents()->controller()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
}

// Tests that focus goes where expected when using reload.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusOnReload) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // Open the new tab, reload.
  browser()->NewTab();
  ui_test_utils::RunAllPendingInMessageLoop();

  browser()->Reload(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  // Focus should stay on the location bar.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

  // Open a regular page, focus the location bar, reload.
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(kSimplePage));
  browser()->FocusLocationBar();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
  browser()->Reload(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));

  // Focus should now be on the tab contents.
  browser()->ShowDownloadsTab();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
}

// Tests that focus goes where expected when using reload on a crashed tab.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_FocusOnReloadCrashedTab) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // Open a regular page, crash, reload.
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(kSimplePage));
  ui_test_utils::CrashTab(browser()->GetSelectedTabContents());
  browser()->Reload(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));

  // Focus should now be on the tab contents.
  browser()->ShowDownloadsTab();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
}

}  // namespace
