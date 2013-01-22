// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/test_server.h"

#if defined(TOOLKIT_VIEWS) || defined(OS_WIN)
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

#if defined(OS_WIN)
#include <Psapi.h>
#include <windows.h>
#include "base/string_util.h"
#endif

using content::InterstitialPage;
using content::NavigationController;
using content::RenderViewHost;
using content::WebContents;

#if defined(OS_MACOSX)
// TODO(suzhe): http://crbug.com/60973
#define MAYBE_FocusTraversal DISABLED_FocusTraversal
#define MAYBE_FocusTraversalOnInterstitial DISABLED_FocusTraversalOnInterstitial
#elif defined(OS_WIN)
// http://crbug.com/109770
#define MAYBE_FocusTraversal FocusTraversal
#define MAYBE_FocusTraversalOnInterstitial DISABLED_FocusTraversalOnInterstitial
#else
#define MAYBE_FocusTraversal FocusTraversal
#define MAYBE_FocusTraversalOnInterstitial FocusTraversalOnInterstitial
#endif

#if defined(OS_LINUX) || defined(OS_MACOSX)
// TODO(jcampan): http://crbug.com/23683 for linux.
// TODO(suzhe): http://crbug.com/49737 for mac.
#define MAYBE_TabsRememberFocusFindInPage DISABLED_TabsRememberFocusFindInPage
#elif defined(OS_WIN)
// Flaky, http://crbug.com/62537.
#define MAYBE_TabsRememberFocusFindInPage DISABLED_TabsRememberFocusFindInPage
#endif

namespace {

// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;

// Maxiumum time to wait until the focus is moved to expected view.
const int kFocusChangeTimeoutMs = 500;

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
  if (len > 1)
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
      if (!GetProcessImageFileName(process, WriteInto(&filename, MAX_PATH),
                                   MAX_PATH)) {
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

// Wait the focus change in message loop.
void CheckFocus(Browser* browser, ViewID id, const base::Time& timeout) {
  if (ui_test_utils::IsViewFocused(browser, id) ||
      base::Time::Now() > timeout) {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  } else {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CheckFocus, browser, id, timeout),
        base::TimeDelta::FromMilliseconds(10));
  }
};

class BrowserFocusTest : public InProcessBrowserTest {
 public:
  BrowserFocusTest() :
#if defined(USE_AURA)
      location_bar_focus_view_id_(VIEW_ID_OMNIBOX)
#else
      location_bar_focus_view_id_(VIEW_ID_LOCATION_BAR)
#endif
  {}

  bool IsViewFocused(ViewID vid) {
    return ui_test_utils::IsViewFocused(browser(), vid);
  }

  void ClickOnView(ViewID vid) {
    ui_test_utils::ClickOnView(browser(), vid);
  }

  bool WaitForFocusChange(ViewID vid) {
    const base::Time timeout = base::Time::Now() +
        base::TimeDelta::FromMilliseconds(kFocusChangeTimeoutMs);
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CheckFocus, browser(), vid, timeout),
        base::TimeDelta::FromMilliseconds(100));
    content::RunMessageLoop();
    return IsViewFocused(vid);
  }

  ViewID location_bar_focus_view_id_;
};

class TestInterstitialPage : public content::InterstitialPageDelegate {
 public:
  TestInterstitialPage(WebContents* tab, bool new_navigation, const GURL& url) {
    FilePath file_path;
    bool r = PathService::Get(chrome::DIR_TEST_DATA, &file_path);
    EXPECT_TRUE(r);
    file_path = file_path.AppendASCII("focus");
    file_path = file_path.AppendASCII(kTypicalPageName);
    r = file_util::ReadFileToString(file_path, &html_contents_);
    EXPECT_TRUE(r);
    interstitial_page_ = InterstitialPage::Create(
        tab, new_navigation, url , this);
    interstitial_page_->Show();
  }

  virtual std::string GetHTMLContents() {
    return html_contents_;
  }

  RenderViewHost* render_view_host() {
    return interstitial_page_->GetRenderViewHostForTesting();
  }

  void DontProceed() {
    interstitial_page_->DontProceed();
  }

  bool HasFocus() {
    return render_view_host()->GetView()->HasFocus();
  }

 private:
  std::string html_contents_;
  InterstitialPage* interstitial_page_;  // Owns us.
};

// Flaky on mac. http://crbug.com/67301.
#if defined(OS_MACOSX)
#define MAYBE_ClickingMovesFocus DISABLED_ClickingMovesFocus
#else
#define MAYBE_ClickingMovesFocus ClickingMovesFocus
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_ClickingMovesFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#if defined(OS_POSIX)
  // It seems we have to wait a little bit for the widgets to spin up before
  // we can start clicking on them.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(kActionDelayMs));
  content::RunMessageLoop();
#endif  // defined(OS_POSIX)

  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));

  ClickOnView(VIEW_ID_TAB_CONTAINER);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  ClickOnView(VIEW_ID_LOCATION_BAR);
  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));
}

// Flaky, http://crbug.com/69034.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_BrowsersRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page.
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  gfx::NativeWindow window = browser()->window()->GetNativeWindow();

  // The focus should be on the Tab contents.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  // Now hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));
  // Hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));

  // The rest of this test does not make sense on Linux because the behavior
  // of Activate() is not well defined and can vary by window manager.
#if defined(OS_WIN)
  // Open a new browser window.
  Browser* browser2 = new Browser(Browser::CreateParams(browser()->profile()));
  ASSERT_TRUE(browser2);
  chrome::AddBlankTabAt(browser2, -1, true);
  browser2->window()->Show();
  ui_test_utils::NavigateToURL(browser2, url);

  gfx::NativeWindow window2 = browser2->window()->GetNativeWindow();
  BrowserView* browser_view2 =
      BrowserView::GetBrowserViewForBrowser(browser2);
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
  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));
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

// Tabs remember focus.
// Disabled, http://crbug.com/62542.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_TabsRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page.
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Create several tabs.
  for (int i = 0; i < 4; ++i) {
    chrome::AddSelectedTabWithURL(browser(), url,
                                  content::PAGE_TRANSITION_TYPED);
  }

  // Alternate focus for the tab.
  const bool kFocusPage[3][5] = {
    { true, true, true, true, false },
    { false, false, false, false, false },
    { false, true, false, true, false }
  };

  for (int i = 1; i < 3; i++) {
    for (int j = 0; j < 5; j++) {
      // Activate the tab.
      browser()->tab_strip_model()->ActivateTabAt(j, true);

      // Activate the location bar or the page.
      if (kFocusPage[i][j]) {
        browser()->tab_strip_model()->GetWebContentsAt(j)->GetView()->Focus();
      } else {
        chrome::FocusLocationBar(browser());
      }
    }

    // Now come back to the tab and check the right view is focused.
    for (int j = 0; j < 5; j++) {
      // Activate the tab.
      browser()->tab_strip_model()->ActivateTabAt(j, true);

      ViewID vid = kFocusPage[i][j] ? VIEW_ID_TAB_CONTAINER :
                                      location_bar_focus_view_id_;
      ASSERT_TRUE(IsViewFocused(vid));
    }

    browser()->tab_strip_model()->ActivateTabAt(0, true);
    // Try the above, but with ctrl+tab. Since tab normally changes focus,
    // this has regressed in the past. Loop through several times to be sure.
    for (int j = 0; j < 15; j++) {
      ViewID vid = kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER :
                                          location_bar_focus_view_id_;
      ASSERT_TRUE(IsViewFocused(vid));

      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
          browser(), ui::VKEY_TAB, true, false, false, false));
    }

    // As above, but with ctrl+shift+tab.
    browser()->tab_strip_model()->ActivateTabAt(4, true);
    for (int j = 14; j >= 0; --j) {
      ViewID vid = kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER :
                                          location_bar_focus_view_id_;
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

  chrome::Find(browser());
  ui_test_utils::FindInPage(chrome::GetActiveWebContents(browser()),
                            ASCIIToUTF16("a"), true, false, NULL, NULL);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Focus the location bar.
  chrome::FocusLocationBar(browser());

  // Create a 2nd tab.
  chrome::AddSelectedTabWithURL(browser(), url, content::PAGE_TRANSITION_TYPED);

  // Focus should be on the recently opened tab page.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Select 1st tab, focus should still be on the location-bar.
  // (bug http://crbug.com/23296)
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));

  // Now open the find box again, switch to another tab and come back, the focus
  // should return to the find box.
  chrome::Find(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

// Background window does not steal focus.
// Flaky, http://crbug.com/62538.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest,
                       DISABLED_BackgroundBrowserDontStealFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // Open a new browser window.
  Browser* browser2 = new Browser(Browser::CreateParams(browser()->profile()));
  ASSERT_TRUE(browser2);
  chrome::AddBlankTabAt(browser2, -1, true);
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

  ASSERT_TRUE(content::ExecuteScript(
      chrome::GetActiveWebContents(unfocused_browser),
      "stealFocus();"));

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

  chrome::FocusLocationBar(browser());

  ASSERT_TRUE(content::ExecuteScript(
      chrome::GetActiveWebContents(browser()),
      "stealFocus();"));

  // Make sure the location bar is still focused.
  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));
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

  chrome::FocusLocationBar(browser());

  const char* kTextElementID = "textEdit";
  const char* kExpElementIDs[] = {
    "",  // Initially no element in the page should be focused
         // (the location bar is focused).
    kTextElementID, "searchButton", "luckyButton", "googleLink", "gmailLink",
    "gmapLink"
  };

  // Test forward focus traversal.
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(base::StringPrintf("outer loop: %d", i));
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));

    // Move the caret to the end, otherwise the next Tab key may not move focus.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_END, false, false, false, false));

    // Now let's press tab to move the focus.
    for (size_t j = 0; j < arraysize(kExpElementIDs); ++j) {
      SCOPED_TRACE(base::StringPrintf("inner loop %" PRIuS, j));
      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(content::ExecuteScriptAndExtractString(
          chrome::GetActiveWebContents(browser()),
          "window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(kExpElementIDs[j], actual.c_str());

      if (j < arraysize(kExpElementIDs) - 1) {
        // If the next element is the kTextElementID, we expect to be
        // notified we have switched to an editable node.
        bool is_editable_node =
            (strcmp(kTextElementID, kExpElementIDs[j + 1]) == 0);
        content::Details<bool> details(&is_editable_node);

        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWaitWithDetails(
            browser(), ui::VKEY_TAB, false, false, false, false,
            content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
            content::NotificationSource(content::Source<RenderViewHost>(
                chrome::GetActiveWebContents(browser())->GetRenderViewHost())),
            details));
      } else {
        // On the last tab key press, the focus returns to the browser.
        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
            browser(), ui::VKEY_TAB, false, false, false, false,
            chrome::NOTIFICATION_FOCUS_RETURNED_TO_BROWSER,
            content::NotificationSource(content::Source<Browser>(browser()))));
      }
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    content::RunAllPendingInMessageLoop();
  }

  // Now let's try reverse focus traversal.
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(base::StringPrintf("outer loop: %d", i));
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));

    // Move the caret to the end, otherwise the next Tab key may not move focus.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_END, false, false, false, false));

    // Now let's press shift-tab to move the focus in reverse.
    for (size_t j = 0; j < arraysize(kExpElementIDs); ++j) {
      SCOPED_TRACE(base::StringPrintf("inner loop: %" PRIuS, j));
      const char* next_element =
          kExpElementIDs[arraysize(kExpElementIDs) - 1 - j];

      if (j < arraysize(kExpElementIDs) - 1) {
        // If the next element is the kTextElementID, we expect to be
        // notified we have switched to an editable node.
        bool is_editable_node = (strcmp(kTextElementID, next_element) == 0);
        content::Details<bool> details(&is_editable_node);

        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWaitWithDetails(
            browser(), ui::VKEY_TAB, false, true, false, false,
            content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
            content::NotificationSource(content::Source<RenderViewHost>(
                chrome::GetActiveWebContents(browser())->GetRenderViewHost())),
            details));
      } else {
        // On the last tab key press, the focus returns to the browser.
        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
            browser(), ui::VKEY_TAB, false, true, false, false,
            chrome::NOTIFICATION_FOCUS_RETURNED_TO_BROWSER,
            content::NotificationSource(content::Source<Browser>(browser()))));
      }

      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(content::ExecuteScriptAndExtractString(
          chrome::GetActiveWebContents(browser()),
          "window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(next_element, actual.c_str());
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    content::RunAllPendingInMessageLoop();
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
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Let's show an interstitial.
  TestInterstitialPage* interstitial_page =
      new TestInterstitialPage(chrome::GetActiveWebContents(browser()),
                               true, GURL("http://interstitial.com"));
  // Give some time for the interstitial to show.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          MessageLoop::QuitClosure(),
                                          base::TimeDelta::FromSeconds(1));
  content::RunMessageLoop();

  chrome::FocusLocationBar(browser());

  const char* kExpElementIDs[] = {
    "",  // Initially no element in the page should be focused
         // (the location bar is focused).
    "textEdit", "searchButton", "luckyButton", "googleLink", "gmailLink",
    "gmapLink"
  };

  // Test forward focus traversal.
  for (int i = 0; i < 2; ++i) {
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));

    // Move the caret to the end, otherwise the next Tab key may not move focus.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_END, false, false, false, false));

    // Now let's press tab to move the focus.
    for (size_t j = 0; j < 7; ++j) {
      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(content::ExecuteScriptAndExtractString(
          interstitial_page->render_view_host(),
          "window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(kExpElementIDs[j], actual.c_str());

      int notification_type;
      content::NotificationSource notification_source =
          content::NotificationService::AllSources();
      if (j < arraysize(kExpElementIDs) - 1) {
        notification_type = content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE;
        notification_source = content::Source<RenderViewHost>(
            interstitial_page->render_view_host());
      } else {
        // On the last tab key press, the focus returns to the browser.
        notification_type = chrome::NOTIFICATION_FOCUS_RETURNED_TO_BROWSER;
        notification_source = content::Source<Browser>(browser());
      }

      ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
          browser(), ui::VKEY_TAB, false, false, false, false,
          notification_type, notification_source));
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    content::RunAllPendingInMessageLoop();
  }

  // Now let's try reverse focus traversal.
  for (int i = 0; i < 2; ++i) {
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));

    // Move the caret to the end, otherwise the next Tab key may not move focus.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_END, false, false, false, false));

    // Now let's press shift-tab to move the focus in reverse.
    for (size_t j = 0; j < 7; ++j) {
      int notification_type;
      content::NotificationSource notification_source =
          content::NotificationService::AllSources();
      if (j < arraysize(kExpElementIDs) - 1) {
        notification_type = content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE;
        notification_source = content::Source<RenderViewHost>(
            interstitial_page->render_view_host());
      } else {
        // On the last tab key press, the focus returns to the browser.
        notification_type = chrome::NOTIFICATION_FOCUS_RETURNED_TO_BROWSER;
        notification_source = content::Source<Browser>(browser());
      }

      ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
          browser(), ui::VKEY_TAB, false, true, false, false,
          notification_type, notification_source));

      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(content::ExecuteScriptAndExtractString(
          interstitial_page->render_view_host(),
          "window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(kExpElementIDs[6 - j], actual.c_str());
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    content::RunAllPendingInMessageLoop();
  }
}

// Focus stays on page with interstitials.
// http://crbug.com/81451
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_InterstitialFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // First we navigate to our test page.
  GURL url = test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Page should have focus.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(chrome::GetActiveWebContents(browser())->GetRenderViewHost()->
              GetView()->HasFocus());

  // Let's show an interstitial.
  TestInterstitialPage* interstitial_page =
      new TestInterstitialPage(chrome::GetActiveWebContents(browser()),
                               true, GURL("http://interstitial.com"));
  // Give some time for the interstitial to show.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          MessageLoop::QuitClosure(),
                                          base::TimeDelta::FromSeconds(1));
  content::RunMessageLoop();

  // The interstitial should have focus now.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(interstitial_page->HasFocus());

  // Hide the interstitial.
  interstitial_page->DontProceed();

  // Focus should be back on the original page.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}

// Make sure Find box can request focus, even when it is already open.
// Disabled due to flakiness. http://crbug.com/67301.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_FindFocusTest) {
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

  ASSERT_TRUE(WaitForFocusChange(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));

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
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Now press Ctrl+F again and focus should move to the Find box.
#if defined(OS_MACOSX)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, false, false, true));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, false, false, false));
#endif

  ASSERT_TRUE(WaitForFocusChange(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

// Makes sure the focus is in the right location when opening the different
// types of tabs.
// Flaky, http://crbug.com/62539.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_TabInitialFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open the history tab, focus should be on the tab contents.
  chrome::ShowHistory(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      chrome::GetActiveWebContents(browser())));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Open the new tab, focus should be on the location bar.
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      chrome::GetActiveWebContents(browser())));
  EXPECT_TRUE(IsViewFocused(location_bar_focus_view_id_));

  // Open the download tab, focus should be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      chrome::GetActiveWebContents(browser())));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Open about:blank, focus should be on the location bar.
  chrome::AddSelectedTabWithURL(browser(), GURL(chrome::kAboutBlankURL),
                                content::PAGE_TRANSITION_LINK);
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      chrome::GetActiveWebContents(browser())));
  EXPECT_TRUE(IsViewFocused(location_bar_focus_view_id_));
}

// Tests that focus goes where expected when using reload.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusOnReload) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // Open the new tab, reload.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::NewTab(browser());
    observer.Wait();
  }
  content::RunAllPendingInMessageLoop();

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &chrome::GetActiveWebContents(browser())->GetController()));
    chrome::Reload(browser(), CURRENT_TAB);
    observer.Wait();
  }
  // Focus should stay on the location bar.
  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));

  // Open a regular page, focus the location bar, reload.
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(kSimplePage));
  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(IsViewFocused(location_bar_focus_view_id_));
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &chrome::GetActiveWebContents(browser())->GetController()));
    chrome::Reload(browser(), CURRENT_TAB);
    observer.Wait();
  }

  // Focus should now be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}

// Tests that focus goes where expected when using reload on a crashed tab.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_FocusOnReloadCrashedTab) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(test_server()->Start());

  // Open a regular page, crash, reload.
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(kSimplePage));
  content::CrashTab(chrome::GetActiveWebContents(browser()));
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &chrome::GetActiveWebContents(browser())->GetController()));
    chrome::Reload(browser(), CURRENT_TAB);
    observer.Wait();
  }

  // Focus should now be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}

// Tests that when a new tab is opened from the omnibox, the focus is moved from
// the omnibox for the current tab.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest,
                       NavigateFromOmniboxIntoNewTab) {
  GURL url("http://www.google.com/");
  GURL url2("http://maps.google.com/");

  // Navigate to url.
  chrome::NavigateParams p(browser(), url, content::PAGE_TRANSITION_LINK);
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.disposition = CURRENT_TAB;
  chrome::Navigate(&p);

  // Focus the omnibox.
  chrome::FocusLocationBar(browser());

  OmniboxEditController* controller =
      browser()->window()->GetLocationBar()->GetLocationEntry()->model()->
          controller();

  // Simulate an alt-enter.
  controller->OnAutocompleteAccept(url2, NEW_FOREGROUND_TAB,
                                   content::PAGE_TRANSITION_TYPED, GURL());

  // Make sure the second tab is selected.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // The tab contents should have the focus in the second tab.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Go back to the first tab. The focus should not be in the omnibox.
  chrome::SelectPreviousTab(browser());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(),
                                            VIEW_ID_LOCATION_BAR));
}

}  // namespace
