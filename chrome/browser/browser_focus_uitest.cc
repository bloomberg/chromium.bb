// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"
#include "views/window/window.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/location_bar/location_bar_view.h"
#include "chrome/browser/views/tab_contents/tab_contents_container.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/gtk/view_id_util.h"
#endif

#if defined(OS_LINUX)
// For some reason we hit an external DNS lookup in this test in Linux but not
// on Windows. TODO(estade): investigate.
#define MAYBE_FocusTraversalOnInterstitial DISABLED_FocusTraversalOnInterstitial
// TODO(jcampan): http://crbug.com/23683
#define MAYBE_TabsRememberFocusFindInPage FAILS_TabsRememberFocusFindInPage
#else
#define MAYBE_FocusTraversalOnInterstitial FocusTraversalOnInterstitial
#define MAYBE_TabsRememberFocusFindInPage TabsRememberFocusFindInPage
#endif

namespace {

// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;

const char kSimplePage[] = "files/focus/page_with_focus.html";
const char kStealFocusPage[] = "files/focus/page_steals_focus.html";
const char kTypicalPage[] = "files/focus/typical_page.html";
const char kTypicalPageName[] = "typical_page.html";

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

  static void HideNativeWindow(gfx::NativeWindow window) {
#if defined(OS_WIN)
    // TODO(jcampan): retrieve the WidgetWin and show/hide on it instead of
    // using Windows API.
    ::ShowWindow(window, SW_HIDE);
#elif defined(TOOLKIT_USES_GTK)
    gtk_widget_hide(GTK_WIDGET(window));
#else
    NOTIMPLEMENTED();
#endif
  }

  static void ShowNativeWindow(gfx::NativeWindow window) {
#if defined(OS_WIN)
    // TODO(jcampan): retrieve the WidgetWin and show/hide on it instead of
    // using Windows API.
    ::ShowWindow(window, SW_SHOW);
#elif defined(TOOLKIT_USES_GTK)
    gtk_widget_hide(GTK_WIDGET(window));
#else
    NOTIMPLEMENTED();
#endif
  }
};

class TestInterstitialPage : public InterstitialPage {
 public:
  TestInterstitialPage(TabContents* tab, bool new_navigation, const GURL& url)
      : InterstitialPage(tab, new_navigation, url),
        waiting_for_dom_response_(false),
        waiting_for_focus_change_(false) {
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

  virtual void DomOperationResponse(const std::string& json_string,
                                    int automation_id) {
    if (waiting_for_dom_response_) {
      dom_response_ = json_string;
      waiting_for_dom_response_ = false;
      MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
      return;
    }
    InterstitialPage::DomOperationResponse(json_string, automation_id);
  }

  std::string GetFocusedElement() {
    std::wstring script = L"window.domAutomationController.setAutomationId(0);"
        L"window.domAutomationController.send(getFocusedElement());";

    render_view_host()->ExecuteJavascriptInWebFrame(L"", script);
    DCHECK(!waiting_for_dom_response_);
    waiting_for_dom_response_ = true;
    ui_test_utils::RunMessageLoop();
    // Remove the JSON extra quotes.
    if (dom_response_.size() >= 2 && dom_response_[0] == '"' &&
        dom_response_[dom_response_.size() - 1] == '"') {
      dom_response_ = dom_response_.substr(1, dom_response_.size() - 2);
    }
    return dom_response_;
  }

  bool HasFocus() {
    return render_view_host()->view()->HasFocus();
  }

  void WaitForFocusChange() {
    waiting_for_focus_change_ = true;
    ui_test_utils::RunMessageLoop();
  }

 protected:
  virtual void FocusedNodeChanged() {
    if (!waiting_for_focus_change_)
      return;

    waiting_for_focus_change_= false;
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

 private:
  std::string html_contents_;

  bool waiting_for_dom_response_;
  bool waiting_for_focus_change_;
  std::string dom_response_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, ClickingMovesFocus) {
#if defined(USE_X11)
  // It seems we have to wait a little bit for the widgets to spin up before
  // we can start clicking on them.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(),
                                          kActionDelayMs);
  ui_test_utils::RunMessageLoop();
#endif

  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

  ClickOnView(VIEW_ID_TAB_CONTAINER);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  ClickOnView(VIEW_ID_LOCATION_BAR);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
}

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BrowsersRememberFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPage(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  gfx::NativeWindow window = browser()->window()->GetNativeHandle();

  // The focus should be on the Tab contents.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
  // Now hide the window, show it again, the focus should not have changed.
  HideNativeWindow(window);
  ShowNativeWindow(window);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  browser()->FocusLocationBar();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
  // Hide the window, show it again, the focus should not have changed.
  HideNativeWindow(window);
  ShowNativeWindow(window);
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
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, TabsRememberFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPage(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Create several tabs.
  for (int i = 0; i < 4; ++i) {
    browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED, -1,
                             TabStripModel::ADD_SELECTED, NULL, std::string());
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

    gfx::NativeWindow window = browser()->window()->GetNativeHandle();
    browser()->SelectTabContentsAt(0, true);
    // Try the above, but with ctrl+tab. Since tab normally changes focus,
    // this has regressed in the past. Loop through several times to be sure.
    for (int j = 0; j < 15; j++) {
      ViewID vid = kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER_FOCUS_VIEW :
                                          VIEW_ID_LOCATION_BAR;
      ASSERT_TRUE(IsViewFocused(vid));

      ui_controls::SendKeyPressNotifyWhenDone(window, base::VKEY_TAB, true,
                                              false, false, false,
                                              new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
    }

    // As above, but with ctrl+shift+tab.
    browser()->SelectTabContentsAt(4, true);
    for (int j = 14; j >= 0; --j) {
      ViewID vid = kFocusPage[i][j % 5] ? VIEW_ID_TAB_CONTAINER_FOCUS_VIEW :
                                          VIEW_ID_LOCATION_BAR;
      ASSERT_TRUE(IsViewFocused(vid));

      ui_controls::SendKeyPressNotifyWhenDone(window, base::VKEY_TAB, true,
                                              true, false, false,
                                              new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
    }
  }
}

// Tabs remember focus with find-in-page box.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_TabsRememberFocusFindInPage) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPage(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->Find();
  ui_test_utils::FindInPage(browser()->GetSelectedTabContents(),
                            ASCIIToUTF16("a"), true, false, NULL);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Focus the location bar.
  browser()->FocusLocationBar();

  // Create a 2nd tab.
  browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED, -1,
                           TabStripModel::ADD_SELECTED, NULL, std::string());

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
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BackgroundBrowserDontStealFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPage(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

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
    ASSERT_TRUE(false);
  }
#elif defined(OS_WIN)
  focused_browser = browser();
  unfocused_browser = browser2;
#endif

  GURL steal_focus_url = server->TestServerPage(kStealFocusPage);
  ui_test_utils::NavigateToURL(unfocused_browser, steal_focus_url);

  // Activate the first browser.
  focused_browser->window()->Activate();

  // Wait for the focus to be stolen by the other browser.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, new MessageLoop::QuitTask(), 2000);
  ui_test_utils::RunMessageLoop();

  // Make sure the first browser is still active.
  EXPECT_TRUE(focused_browser->window()->IsActive());

  // Close the 2nd browser to avoid a DCHECK().
  browser2->window()->Close();
}

// Page cannot steal focus when focus is on location bar.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, LocationBarLockFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // Open the page that steals focus.
  GURL url = server->TestServerPage(kStealFocusPage);
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->FocusLocationBar();

  // Wait for the page to steal focus.
  PlatformThread::Sleep(2000);

  // Make sure the location bar is still focused.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
}

// Focus traversal on a regular page.
// Note that this test relies on a notification from the renderer that the
// focus has changed in the page.  The notification in the renderer may change
// at which point this test would fail (see comment in
// RenderWidget::didFocus()).
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusTraversal) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPage(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->FocusLocationBar();

  const char* kExpElementIDs[] = {
    "",  // Initially no element in the page should be focused
         // (the location bar is focused).
    "textEdit", "searchButton", "luckyButton", "googleLink", "gmailLink",
    "gmapLink"
  };

  gfx::NativeWindow window = browser()->window()->GetNativeHandle();

  // Test forward focus traversal.
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(StringPrintf("outer loop: %d", i));
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

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

      ASSERT_TRUE(ui_controls::SendKeyPress(window, base::VKEY_TAB,
                                            false, false, false, false));

      if (j < arraysize(kExpElementIDs) - 1) {
        ui_test_utils::WaitForFocusChange(browser()->GetSelectedTabContents()->
            render_view_host());
      } else {
        // On the last tab key press, the focus returns to the browser.
        ui_test_utils::WaitForFocusInBrowser(browser());
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

    // Now let's press shift-tab to move the focus in reverse.
    for (size_t j = 0; j < 7; ++j) {
      SCOPED_TRACE(StringPrintf("inner loop: %" PRIuS, j));
      ASSERT_TRUE(ui_controls::SendKeyPress(window, base::VKEY_TAB,
                                            false, true, false, false));

      if (j < arraysize(kExpElementIDs) - 1) {
        ui_test_utils::WaitForFocusChange(browser()->GetSelectedTabContents()->
            render_view_host());
      } else {
        // On the last tab key press, the focus returns to the browser.
        ui_test_utils::WaitForFocusInBrowser(browser());
      }

      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"",
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

// Focus traversal while an interstitial is showing.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, MAYBE_FocusTraversalOnInterstitial) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPage(kSimplePage);
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

  gfx::NativeWindow window = browser()->window()->GetNativeHandle();

  // Test forward focus traversal.
  for (int i = 0; i < 2; ++i) {
    // Location bar should be focused.
    ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

    // Now let's press tab to move the focus.
    for (size_t j = 0; j < 7; ++j) {
      // Let's make sure the focus is on the expected element in the page.
      std::string actual = interstitial_page->GetFocusedElement();
      ASSERT_STREQ(kExpElementIDs[j], actual.c_str());

      ASSERT_TRUE(ui_controls::SendKeyPress(window, base::VKEY_TAB,
                                            false, false, false, false));

      if (j < arraysize(kExpElementIDs) - 1) {
        interstitial_page->WaitForFocusChange();
      } else {
        // On the last tab key press, the focus returns to the browser.
        ui_test_utils::WaitForFocusInBrowser(browser());
      }
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

    // Now let's press shift-tab to move the focus in reverse.
    for (size_t j = 0; j < 7; ++j) {
      ASSERT_TRUE(ui_controls::SendKeyPress(window, base::VKEY_TAB,
                                            false, true, false, false));

      if (j < arraysize(kExpElementIDs) - 1) {
        interstitial_page->WaitForFocusChange();
      } else {
        // On the last tab key press, the focus returns to the browser.
        ui_test_utils::WaitForFocusInBrowser(browser());
      }

      // Let's make sure the focus is on the expected element in the page.
      std::string actual = interstitial_page->GetFocusedElement();
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
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPage(kSimplePage);
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
  HTTPTestServer* server = StartHTTPServer();

  // Open some page (any page that doesn't steal focus).
  GURL url = server->TestServerPage(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);

  gfx::NativeWindow window = browser()->window()->GetNativeHandle();

  // Press Ctrl+F, which will make the Find box open and request focus.
  ui_controls::SendKeyPressNotifyWhenDone(window, base::VKEY_F, true,
                                          false, false, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

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
  ui_controls::SendKeyPressNotifyWhenDone(window, base::VKEY_F, true,
                                          false, false, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Set focus to the page.
  ClickOnView(VIEW_ID_TAB_CONTAINER);
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Now press Ctrl+F again and focus should move to the Find box.
  ui_controls::SendKeyPressNotifyWhenDone(window, base::VKEY_F, true, false,
                                          false, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  // See remark above on why we wait.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, new MessageLoop::QuitTask(), kActionDelayMs);
  ui_test_utils::RunMessageLoop();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

// Makes sure the focus is in the right location when opening the different
// types of tabs.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, TabInitialFocus) {
  // Open the history tab, focus should be on the tab contents.
  browser()->ShowHistoryTab();

  ui_test_utils::RunAllPendingInMessageLoop();

  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Open the new tab, focus should be on the location bar.
  browser()->NewTab();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

  // Open the download tab, focus should be on the tab contents.
  browser()->ShowDownloadsTab();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Open about:blank, focus should be on the location bar.
  browser()->AddTabWithURL(GURL("about:blank"), GURL(), PageTransition::LINK,
                           -1, TabStripModel::ADD_SELECTED, NULL,
                           std::string());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
}

// Tests that focus goes where expected when using reload.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusOnReload) {
  HTTPTestServer* server = StartHTTPServer();

  // Open the new tab, reload.
  browser()->NewTab();

  ui_test_utils::RunAllPendingInMessageLoop();

  browser()->Reload(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  // Focus should stay on the location bar.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));

  // Open a regular page, focus the location bar, reload.
  ui_test_utils::NavigateToURL(browser(), server->TestServerPage(kSimplePage));
  browser()->FocusLocationBar();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_LOCATION_BAR));
  browser()->Reload(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  // Focus should now be on the tab contents.
  browser()->ShowDownloadsTab();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
}

// Tests that focus goes where expected when using reload on a crashed tab.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusOnReloadCrashedTab) {
  HTTPTestServer* server = StartHTTPServer();

  // Open a regular page, crash, reload.
  ui_test_utils::NavigateToURL(browser(), server->TestServerPage(kSimplePage));
  ui_test_utils::CrashTab(browser()->GetSelectedTabContents());
  browser()->Reload(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  // Focus should now be on the tab contents.
  browser()->ShowDownloadsTab();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));
}
