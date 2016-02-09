// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using content::RenderViewHost;
using content::WebContents;

#if defined(OS_LINUX) || defined(OS_MACOSX)
// TODO(jcampan): http://crbug.com/23683 for linux.
// TODO(suzhe): http://crbug.com/49737 for mac.
#define MAYBE_TabsRememberFocusFindInPage DISABLED_TabsRememberFocusFindInPage
#elif defined(OS_WIN)
// Flaky, http://crbug.com/62537.
#define MAYBE_TabsRememberFocusFindInPage DISABLED_TabsRememberFocusFindInPage
#endif

namespace {

#if defined(OS_POSIX)
// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;
#endif

const char kSimplePage[] = "/focus/page_with_focus.html";
const char kStealFocusPage[] = "/focus/page_steals_focus.html";
const char kTypicalPage[] = "/focus/typical_page.html";

class BrowserFocusTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  bool IsViewFocused(ViewID vid) {
    return ui_test_utils::IsViewFocused(browser(), vid);
  }

  void ClickOnView(ViewID vid) {
    ui_test_utils::ClickOnView(browser(), vid);
  }

  void TestFocusTraversal(RenderViewHost* render_view_host, bool reverse) {
    const char kGetFocusedElementJS[] =
        "window.domAutomationController.send(getFocusedElement());";
    const char* kExpectedIDs[] = { "textEdit", "searchButton", "luckyButton",
                                   "googleLink", "gmailLink", "gmapLink" };
    SCOPED_TRACE(base::StringPrintf("TestFocusTraversal: reverse=%d", reverse));
    ui::KeyboardCode key = ui::VKEY_TAB;
#if defined(OS_MACOSX)
    // TODO(msw): Mac requires ui::VKEY_BACKTAB for reverse cycling. Sigh...
    key = reverse ? ui::VKEY_BACKTAB : ui::VKEY_TAB;
#elif defined(OS_WIN)
    // This loop times out on Windows XP with no output. http://crbug.com/376635
    if (base::win::GetVersion() < base::win::VERSION_VISTA)
      return;
#endif

    // Loop through the focus chain twice for good measure.
    for (size_t i = 0; i < 2; ++i) {
      SCOPED_TRACE(base::StringPrintf("focus outer loop: %" PRIuS, i));
      ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

      // Mac requires an extra Tab key press to traverse the app menu button
      // iff "Full Keyboard Access" is enabled. In reverse, four Tab key presses
      // are required to traverse the back/forward buttons and the tab strip.
#if defined(OS_MACOSX)
      if (ui_controls::IsFullKeyboardAccessEnabled()) {
        ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
            browser(), key, false, reverse, false, false));
        if (reverse) {
          for (int j = 0; j < 3; ++j) {
            ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
                browser(), key, false, reverse, false, false));
          }
        }
      }
#endif

      if (reverse) {
        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
            browser(), key, false, reverse, false, false,
            content::NOTIFICATION_ALL,
            content::NotificationService::AllSources()));
      }

      for (size_t j = 0; j < arraysize(kExpectedIDs); ++j) {
        SCOPED_TRACE(base::StringPrintf("focus inner loop %" PRIuS, j));
        const size_t index = reverse ? arraysize(kExpectedIDs) - 1 - j : j;
        // The details are the node's editable state, i.e. true for "textEdit".
        bool is_editable_node = index == 0;

        // Press Tab (or Shift+Tab) and check the focused element id.
        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWaitWithDetails(
            browser(), key, false, reverse, false, false,
            content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
            content::Source<RenderViewHost>(render_view_host),
            content::Details<bool>(&is_editable_node)));
        std::string focused_id;
        EXPECT_TRUE(content::ExecuteScriptAndExtractString(
            render_view_host, kGetFocusedElementJS, &focused_id));
        EXPECT_STREQ(kExpectedIDs[index], focused_id.c_str());
      }

#if defined(OS_MACOSX)
      // TODO(msw): Mac doesn't post NOTIFICATION_FOCUS_RETURNED_TO_BROWSER and
      // would also apparently require extra Tab key presses here. Sigh...
      chrome::FocusLocationBar(browser());
#else
      // On the last Tab key press, focus returns to the browser.
      ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
          browser(), key, false, reverse, false, false,
          chrome::NOTIFICATION_FOCUS_RETURNED_TO_BROWSER,
          content::Source<Browser>(browser())));
      EXPECT_TRUE(
          IsViewFocused(reverse ? VIEW_ID_OMNIBOX : VIEW_ID_LOCATION_ICON));

      ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
          browser(), key, false, reverse, false, false,
          content::NOTIFICATION_ALL,
          content::NotificationService::AllSources()));
#endif
      content::RunAllPendingInMessageLoop();
      EXPECT_TRUE(
          IsViewFocused(reverse ? VIEW_ID_LOCATION_ICON : VIEW_ID_OMNIBOX));
      if (reverse) {
        ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
            browser(), key, false, false, false, false,
            content::NOTIFICATION_ALL,
            content::NotificationService::AllSources()));
      }
    }
  }
};

// A test interstitial page with typical HTML contents.
class TestInterstitialPage : public content::InterstitialPageDelegate {
 public:
  explicit TestInterstitialPage(WebContents* tab) {
    base::FilePath file_path;
    bool success = PathService::Get(chrome::DIR_TEST_DATA, &file_path);
    EXPECT_TRUE(success);
    file_path = file_path.AppendASCII("focus/typical_page.html");
    success = base::ReadFileToString(file_path, &html_contents_);
    EXPECT_TRUE(success);
    interstitial_page_ = content::InterstitialPage::Create(
        tab, true, GURL("http://interstitial.com"), this);

    // Show the interstitial and delay return until it has attached.
    interstitial_page_->Show();
    content::WaitForInterstitialAttach(tab);

    EXPECT_TRUE(tab->ShowingInterstitialPage());
  }

  std::string GetHTMLContents() override { return html_contents_; }

  RenderViewHost* render_view_host() {
    return interstitial_page_->GetMainFrame()->GetRenderViewHost();
  }

  void DontProceed() { interstitial_page_->DontProceed(); }

  bool HasFocus() {
    return render_view_host()->GetWidget()->GetView()->HasFocus();
  }

 private:
  std::string html_contents_;
  content::InterstitialPage* interstitial_page_;  // Owns this.
  DISALLOW_COPY_AND_ASSIGN(TestInterstitialPage);
};

// Flaky on Mac (http://crbug.com/67301).
#if defined(OS_MACOSX)
#define MAYBE_ClickingMovesFocus DISABLED_ClickingMovesFocus
#else
// If this flakes, disable and log details in http://crbug.com/523255.
#define MAYBE_ClickingMovesFocus ClickingMovesFocus
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_ClickingMovesFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#if defined(OS_POSIX)
  // It seems we have to wait a little bit for the widgets to spin up before
  // we can start clicking on them.
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(kActionDelayMs));
  content::RunMessageLoop();
#endif  // defined(OS_POSIX)

  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  ClickOnView(VIEW_ID_TAB_CONTAINER);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  ClickOnView(VIEW_ID_OMNIBOX);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Flaky, http://crbug.com/69034.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_BrowsersRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
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
}

// Tabs remember focus.
// Disabled, http://crbug.com/62542.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_TabsRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Create several tabs.
  for (int i = 0; i < 4; ++i) {
    chrome::AddSelectedTabWithURL(browser(), url,
                                  ui::PAGE_TRANSITION_TYPED);
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
        browser()->tab_strip_model()->GetWebContentsAt(j)->Focus();
      } else {
        chrome::FocusLocationBar(browser());
      }
    }

    // Now come back to the tab and check the right view is focused.
    for (int j = 0; j < 5; j++) {
      // Activate the tab.
      browser()->tab_strip_model()->ActivateTabAt(j, true);

      ViewID vid = kFocusPage[i][j] ? VIEW_ID_TAB_CONTAINER : VIEW_ID_OMNIBOX;
      ASSERT_TRUE(IsViewFocused(vid));
    }

    browser()->tab_strip_model()->ActivateTabAt(0, true);
    // Try the above, but with ctrl+tab. Since tab normally changes focus,
    // this has regressed in the past. Loop through several times to be sure.
    for (int j = 0; j < 15; j++) {
      ViewID vid = kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER :
                                          VIEW_ID_OMNIBOX;
      ASSERT_TRUE(IsViewFocused(vid));

      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
          browser(), ui::VKEY_TAB, true, false, false, false));
    }

    // As above, but with ctrl+shift+tab.
    browser()->tab_strip_model()->ActivateTabAt(4, true);
    for (int j = 14; j >= 0; --j) {
      ViewID vid = kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER :
                                          VIEW_ID_OMNIBOX;
      ASSERT_TRUE(IsViewFocused(vid));

      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
          browser(), ui::VKEY_TAB, true, true, false, false));
    }
  }
}

// Tabs remember focus with find-in-page box.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_TabsRememberFocusFindInPage) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::Find(browser());
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::ASCIIToUTF16("a"), true, false, NULL, NULL);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Focus the location bar.
  chrome::FocusLocationBar(browser());

  // Create a 2nd tab.
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);

  // Focus should be on the recently opened tab page.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Select 1st tab, focus should still be on the location-bar.
  // (bug http://crbug.com/23296)
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

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

  // Open a new browser window.
  Browser* browser2 = new Browser(Browser::CreateParams(browser()->profile()));

  ASSERT_TRUE(browser2);
  chrome::AddTabAt(browser2, GURL(), -1, true);
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

  const GURL steal_focus_url = embedded_test_server()->GetURL(kStealFocusPage);
  ui_test_utils::NavigateToURL(unfocused_browser, steal_focus_url);

  // Activate the first browser.
  focused_browser->window()->Activate();

  ASSERT_TRUE(content::ExecuteScript(
      unfocused_browser->tab_strip_model()->GetActiveWebContents(),
      "stealFocus();"));

  // Make sure the first browser is still active.
  EXPECT_TRUE(focused_browser->window()->IsActive());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): http://crbug.com/163931
#define MAYBE_LocationBarLockFocus DISABLED_LocationBarLockFocus
#else
#define MAYBE_LocationBarLockFocus LocationBarLockFocus
#endif

// Page cannot steal focus when focus is on location bar.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_LocationBarLockFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open the page that steals focus.
  const GURL url = embedded_test_server()->GetURL(kStealFocusPage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::FocusLocationBar(browser());

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "stealFocus();"));

  // Make sure the location bar is still focused.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// Test forward and reverse focus traversal on a typical page.
// Disabled for Mac because it is flaky on "Mac10.9 Tests (dbg)",
// see https://crbug.com/60973.
#if defined(OS_MACOSX)
#define MAYBE_FocusTraversal DISABLED_FocusTraversal
#else
#define MAYBE_FocusTraversal FocusTraversal
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FocusTraversal) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  chrome::FocusLocationBar(browser());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NO_FATAL_FAILURE(TestFocusTraversal(tab->GetRenderViewHost(), false));
  EXPECT_NO_FATAL_FAILURE(TestFocusTraversal(tab->GetRenderViewHost(), true));
}

// Test forward and reverse focus traversal while an interstitial is showing.
// Disabled, see http://crbug.com/60973
IN_PROC_BROWSER_TEST_F(BrowserFocusTest,
                       DISABLED_FocusTraversalOnInterstitial) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Create and show a test interstitial page.
  TestInterstitialPage* interstitial_page = new TestInterstitialPage(
      browser()->tab_strip_model()->GetActiveWebContents());
  content::RenderViewHost* host = interstitial_page->render_view_host();

  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  chrome::FocusLocationBar(browser());
  EXPECT_NO_FATAL_FAILURE(TestFocusTraversal(host, false));
  EXPECT_NO_FATAL_FAILURE(TestFocusTraversal(host, true));
}

// Test the transfer of focus when an interstitial is shown and hidden.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, InterstitialFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(tab->GetRenderViewHost()->GetWidget()->GetView()->HasFocus());

  // Create and show a test interstitial page; it should gain focus.
  TestInterstitialPage* interstitial_page = new TestInterstitialPage(tab);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(interstitial_page->HasFocus());

  // Hide the interstitial; the original page should gain focus.
  interstitial_page->DontProceed();
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(tab->GetRenderViewHost()->GetWidget()->GetView()->HasFocus());
}

// Test that find-in-page UI can request focus, even when it is already open.
#if defined(OS_MACOSX)
#define MAYBE_FindFocusTest DISABLED_FindFocusTest
#else
// If this flakes, disable and log details in http://crbug.com/523255.
#define MAYBE_FindFocusTest FindFocusTest
#endif
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FindFocusTest) {
  chrome::DisableFindBarAnimationsDuringTesting(true);
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const GURL url = embedded_test_server()->GetURL(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  chrome::ShowFindBar(browser());
  EXPECT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  chrome::ShowFindBar(browser());
  EXPECT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  ClickOnView(VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  chrome::ShowFindBar(browser());
  EXPECT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

// Makes sure the focus is in the right location when opening the different
// types of tabs.
// Flaky, http://crbug.com/62539.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_TabInitialFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Open the history tab, focus should be on the tab contents.
  chrome::ShowHistory(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Open the new tab, focus should be on the location bar.
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Open the download tab, focus should be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Open about:blank, focus should be on the location bar.
  chrome::AddSelectedTabWithURL(
      browser(), GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_LINK);
  ASSERT_NO_FATAL_FAILURE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): http://crbug.com/163931
#define MAYBE_FocusOnReload DISABLED_FocusOnReload
#else
#define MAYBE_FocusOnReload FocusOnReload
#endif

// Tests that focus goes where expected when using reload.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FocusOnReload) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

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
        content::Source<content::NavigationController>(
            &browser()->tab_strip_model()->GetActiveWebContents()->
                GetController()));
    chrome::Reload(browser(), CURRENT_TAB);
    observer.Wait();
  }
  // Focus should stay on the location bar.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Open a regular page, focus the location bar, reload.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));
  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(
            &browser()->tab_strip_model()->GetActiveWebContents()->
                GetController()));
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

  // Open a regular page, crash, reload.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));
  content::CrashTab(browser()->tab_strip_model()->GetActiveWebContents());
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(
            &browser()->tab_strip_model()->GetActiveWebContents()->
                GetController()));
    chrome::Reload(browser(), CURRENT_TAB);
    observer.Wait();
  }

  // Focus should now be on the tab contents.
  chrome::ShowDownloads(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}

// Tests that focus goes to frame after crashed tab.
// TODO(shrikant): Find out where the focus should be deterministically.
// Currently focused_view after crash seem to be non null in debug mode
// (invalidated pointer 0xcccccc).
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_FocusAfterCrashedTab) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  content::CrashTab(browser()->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
}

// Tests that when a new tab is opened from the omnibox, the focus is moved from
// the omnibox for the current tab.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, NavigateFromOmniboxIntoNewTab) {
  GURL url("http://www.google.com/");
  GURL url2("http://maps.google.com/");

  // Navigate to url.
  chrome::NavigateParams p(browser(), url, ui::PAGE_TRANSITION_LINK);
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.disposition = CURRENT_TAB;
  chrome::Navigate(&p);

  // Focus the omnibox.
  chrome::FocusLocationBar(browser());

  OmniboxEditController* controller = browser()->window()->GetLocationBar()->
      GetOmniboxView()->model()->controller();

  // Simulate an alt-enter.
  controller->OnAutocompleteAccept(url2, NEW_FOREGROUND_TAB,
                                   ui::PAGE_TRANSITION_TYPED);

  // Make sure the second tab is selected.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // The tab contents should have the focus in the second tab.
  EXPECT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  // Go back to the first tab. The focus should not be in the omnibox.
  chrome::SelectPreviousTab(browser());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));
}

// This functionality is currently broken. http://crbug.com/304865.
//
//#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
//// TODO(erg): http://crbug.com/163931
//#define MAYBE_FocusOnNavigate DISABLED_FocusOnNavigate
//#else
//#define MAYBE_FocusOnNavigate FocusOnNavigate
//#endif

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, DISABLED_FocusOnNavigate) {
  // Needed on Mac.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // Load the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Navigate to another page.
  const base::FilePath::CharType* kEmptyFile = FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kEmptyFile)));
  ui_test_utils::NavigateToURL(browser(), file_url);

  ClickOnView(VIEW_ID_TAB_CONTAINER);

  // Navigate back.  Should focus the location bar.
  {
    content::WindowedNotificationObserver back_nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
    chrome::GoBack(browser(), CURRENT_TAB);
    back_nav_observer.Wait();
  }

  EXPECT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // Navigate forward.  Shouldn't focus the location bar.
  ClickOnView(VIEW_ID_TAB_CONTAINER);
  {
    content::WindowedNotificationObserver forward_nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
    chrome::GoForward(browser(), CURRENT_TAB);
    forward_nav_observer.Wait();
  }

  EXPECT_FALSE(IsViewFocused(VIEW_ID_OMNIBOX));
}

}  // namespace
