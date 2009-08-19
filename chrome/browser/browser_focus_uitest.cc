// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/keyboard_codes.h"
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
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/tab_contents/tab_contents_container.h"
#endif

#if defined(OS_LINUX)
#include "chrome/browser/gtk/view_id_util.h"
#endif

namespace {

// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;

const wchar_t kSimplePage[] = L"files/focus/page_with_focus.html";
const wchar_t kStealFocusPage[] = L"files/focus/page_steals_focus.html";
const wchar_t kTypicalPage[] = L"files/focus/typical_page.html";
const wchar_t kTypicalPageName[] = L"typical_page.html";

class BrowserFocusTest : public InProcessBrowserTest {
 public:
  BrowserFocusTest() {
    set_show_window(true);
    EnableDOMAutomation();
  }

  void CheckViewHasFocus(ViewID vid) {
    BrowserWindow* browser_window = browser()->window();
    ASSERT_TRUE(browser_window);
    gfx::NativeWindow window = browser_window->GetNativeHandle();
    ASSERT_TRUE(window);
#if defined(OS_WIN)
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManagerForNativeView(window);
    ASSERT_TRUE(focus_manager);
    EXPECT_EQ(reinterpret_cast<BrowserView*>(browser_window)->GetViewByID(vid),
              focus_manager->GetFocusedView()) << "For view id " << vid;
#elif defined(OS_LINUX)
    GtkWidget* widget = ViewIDUtil::GetWidget(GTK_WIDGET(window), vid);
    ASSERT_TRUE(widget);
    EXPECT_TRUE(WidgetInFocusChain(GTK_WIDGET(window), widget));
#else
    NOTIMPLEMENTED();
#endif
  }

  static void HideNativeWindow(gfx::NativeWindow window) {
#if defined(OS_WIN)
    // TODO(jcampan): retrieve the WidgetWin and show/hide on it instead of
    // using Windows API.
    ::ShowWindow(window, SW_HIDE);
#elif defined(OS_LINUX)
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
#elif defined(OS_LINUX)
    gtk_widget_hide(GTK_WIDGET(window));
#else
    NOTIMPLEMENTED();
#endif
  }

 private:
#if defined(OS_LINUX)
  // Check if the focused widget for |root| is |target| or a child of |target|.
  static bool WidgetInFocusChain(GtkWidget* root, GtkWidget* target) {
    GtkWidget* iter = root;

    while (iter) {
      if (iter == target)
        return true;

      if (!GTK_IS_CONTAINER(iter))
        return false;

      iter = GTK_CONTAINER(iter)->focus_child;
    }

    return false;
  }
#endif
};

class TestInterstitialPage : public InterstitialPage {
 public:
  TestInterstitialPage(TabContents* tab, bool new_navigation, const GURL& url)
      : InterstitialPage(tab, new_navigation, url),
        waiting_for_dom_response_(false) {
    std::wstring file_path;
    bool r = PathService::Get(chrome::DIR_TEST_DATA, &file_path);
    EXPECT_TRUE(r);
    file_util::AppendToPath(&file_path, L"focus");
    file_util::AppendToPath(&file_path, kTypicalPageName);
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

 private:
  std::string html_contents_;

  bool waiting_for_dom_response_;
  std::string dom_response_;

};

}  // namespace

// TODO(estade): port.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, ClickingMovesFocus) {
  browser()->AddTabWithURL(GURL("about:blank"), GURL(), PageTransition::LINK,
                           true, -1, false, NULL);
  CheckViewHasFocus(VIEW_ID_LOCATION_BAR);

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ui_controls::MoveMouseToCenterAndPress(
      browser_view->GetTabContentsContainerView(),
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
  CheckViewHasFocus(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);

  ui_controls::MoveMouseToCenterAndPress(
      browser_view->GetLocationBarView(),
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
  CheckViewHasFocus(VIEW_ID_LOCATION_BAR);
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BrowsersRememberFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  gfx::NativeWindow window = browser()->window()->GetNativeHandle();

  // The focus should be on the Tab contents.
  CheckViewHasFocus(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);
  // Now hide the window, show it again, the focus should not have changed.
  HideNativeWindow(window);
  ShowNativeWindow(window);
  CheckViewHasFocus(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);

  browser()->FocusLocationBar();
  CheckViewHasFocus(VIEW_ID_LOCATION_BAR);
  // Hide the window, show it again, the focus should not have changed.
  HideNativeWindow(window);
  ShowNativeWindow(window);
  CheckViewHasFocus(VIEW_ID_LOCATION_BAR);

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
  CheckViewHasFocus(VIEW_ID_LOCATION_BAR);
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
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Create several tabs.
  for (int i = 0; i < 4; ++i) {
    browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED, true, -1,
                             false, NULL);
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
      CheckViewHasFocus(vid);
    }
  }
}

// Background window does not steal focus.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BackgroundBrowserDontStealFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Open a new browser window.
  Browser* browser2 = Browser::Create(browser()->profile());
  ASSERT_TRUE(browser2);
  browser2->tabstrip_model()->delegate()->AddBlankTab(true);
  browser2->window()->Show();

  Browser* focused_browser;
  Browser* unfocused_browser;
#if defined(OS_LINUX)
  // On Linux, calling Activate() is not guaranteed to move focus, so we have
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

  GURL steal_focus_url = server->TestServerPageW(kStealFocusPage);
  ui_test_utils::NavigateToURL(unfocused_browser, steal_focus_url);

  // Activate the first browser.
  focused_browser->window()->Activate();

  // Wait for the focus to be stolen by the other browser.
  PlatformThread::Sleep(2000);

  // Make sure the first browser is still active.
  EXPECT_TRUE(focused_browser->window()->IsActive());

  // Close the 2nd browser to avoid a DCHECK().
  browser2->window()->Close();
}

// Page cannot steal focus when focus is on location bar.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, LocationBarLockFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // Open the page that steals focus.
  GURL url = server->TestServerPageW(kStealFocusPage);
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->FocusLocationBar();

  // Wait for the page to steal focus.
  PlatformThread::Sleep(2000);

  // Make sure the location bar is still focused.
  CheckViewHasFocus(VIEW_ID_LOCATION_BAR);
}

// Focus traversal on a regular page.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusTraversal) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kTypicalPage);
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
    // Location bar should be focused.
    CheckViewHasFocus(VIEW_ID_LOCATION_BAR);

    // Now let's press tab to move the focus.
    for (int j = 0; j < 7; ++j) {
      // Let's make sure the focus is on the expected element in the page.
      std::string actual;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"",
          L"window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(kExpElementIDs[j], actual.c_str());

      ui_controls::SendKeyPressNotifyWhenDone(window, base::VKEY_TAB, false,
                                              false, false,
                                              new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
      // Ideally, we wouldn't sleep here and instead would use the event
      // processed ack notification from the renderer.  I am reluctant to create
      // a new notification/callback for that purpose just for this test.
      PlatformThread::Sleep(kActionDelayMs);
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }

  // Now let's try reverse focus traversal.
  for (int i = 0; i < 3; ++i) {
    // Location bar should be focused.
    CheckViewHasFocus(VIEW_ID_LOCATION_BAR);

    // Now let's press shift-tab to move the focus in reverse.
    for (int j = 0; j < 7; ++j) {
      ui_controls::SendKeyPressNotifyWhenDone(window, base::VKEY_TAB, false,
                                              true, false,
                                              new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
      PlatformThread::Sleep(kActionDelayMs);

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
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }
}

#if defined(OS_WIN)
// Focus traversal while an interstitial is showing.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusTraversalOnInterstitial) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(hwnd);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(hwnd);

  // Focus should be on the page.
  EXPECT_EQ(browser_view->GetTabContentsContainerView(),
            focus_manager->GetFocusedView());

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

  // Click on the location bar.
  LocationBarView* location_bar = browser_view->GetLocationBarView();
  ui_controls::MoveMouseToCenterAndPress(location_bar,
                                         ui_controls::LEFT,
                                         ui_controls::DOWN | ui_controls::UP,
                                         new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  const char* kExpElementIDs[] = {
    "",  // Initially no element in the page should be focused
         // (the location bar is focused).
    "textEdit", "searchButton", "luckyButton", "googleLink", "gmailLink",
    "gmapLink"
  };

  // Test forward focus traversal.
  for (int i = 0; i < 2; ++i) {
    // Location bar should be focused.
    EXPECT_EQ(location_bar, focus_manager->GetFocusedView());

    // Now let's press tab to move the focus.
    for (int j = 0; j < 7; ++j) {
      // Let's make sure the focus is on the expected element in the page.
      std::string actual = interstitial_page->GetFocusedElement();
      ASSERT_STREQ(kExpElementIDs[j], actual.c_str());

      ui_controls::SendKeyPressNotifyWhenDone(NULL, base::VKEY_TAB, false,
                                              false, false,
                                              new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
      // Ideally, we wouldn't sleep here and instead would use the event
      // processed ack notification from the renderer.  I am reluctant to create
      // a new notification/callback for that purpose just for this test.
      ::Sleep(kActionDelayMs);
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }

  // Now let's try reverse focus traversal.
  for (int i = 0; i < 2; ++i) {
    // Location bar should be focused.
    EXPECT_EQ(location_bar, focus_manager->GetFocusedView());

    // Now let's press shift-tab to move the focus in reverse.
    for (int j = 0; j < 7; ++j) {
      ui_controls::SendKeyPressNotifyWhenDone(NULL, base::VKEY_TAB, false,
                                              true, false,
                                              new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
      ::Sleep(kActionDelayMs);

      // Let's make sure the focus is on the expected element in the page.
      std::string actual = interstitial_page->GetFocusedElement();
      ASSERT_STREQ(kExpElementIDs[6 - j], actual.c_str());
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }
}

// Focus stays on page with interstitials.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, InterstitialFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(hwnd);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(hwnd);

  // Page should have focus.
  EXPECT_EQ(browser_view->GetTabContentsContainerView(),
            focus_manager->GetFocusedView());
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
  EXPECT_EQ(browser_view->GetTabContentsContainerView(),
            focus_manager->GetFocusedView());
  EXPECT_TRUE(interstitial_page->HasFocus());

  // Hide the interstitial.
  interstitial_page->DontProceed();

  // Focus should be back on the original page.
  EXPECT_EQ(browser_view->GetTabContentsContainerView(),
            focus_manager->GetFocusedView());
  EXPECT_TRUE(browser()->GetSelectedTabContents()->render_view_host()->view()->
      HasFocus());
}

// Make sure Find box can request focus, even when it is already open.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FindFocusTest) {
  HTTPTestServer* server = StartHTTPServer();

  // Open some page (any page that doesn't steal focus).
  GURL url = server->TestServerPageW(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);

  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(hwnd);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(hwnd);
  LocationBarView* location_bar = browser_view->GetLocationBarView();

  // Press Ctrl+F, which will make the Find box open and request focus.
  ui_controls::SendKeyPressNotifyWhenDone(NULL, base::VKEY_F, true,
                                          false, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  // Ideally, we wouldn't sleep here and instead would intercept the
  // RenderViewHostDelegate::HandleKeyboardEvent() callback.  To do that, we
  // could create a RenderViewHostDelegate wrapper and hook-it up by either:
  // - creating a factory used to create the delegate
  // - making the test a private and overwriting the delegate member directly.
  ::Sleep(kActionDelayMs);
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  views::View* focused_view = focus_manager->GetFocusedView();
  ASSERT_TRUE(focused_view != NULL);
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, focused_view->GetID());

  // Click on the location bar.
  ui_controls::MoveMouseToCenterAndPress(location_bar,
                                         ui_controls::LEFT,
                                         ui_controls::DOWN | ui_controls::UP,
                                         new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  // Make sure the location bar is focused.
  EXPECT_EQ(location_bar, focus_manager->GetFocusedView());

  // Now press Ctrl+F again and focus should move to the Find box.
  ui_controls::SendKeyPressNotifyWhenDone(NULL, base::VKEY_F, true,
                                          false, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
  focused_view = focus_manager->GetFocusedView();
  ASSERT_TRUE(focused_view != NULL);
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, focused_view->GetID());

  // Set focus to the page.
  ui_controls::MoveMouseToCenterAndPress(
      browser_view->GetTabContentsContainerView(),
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(browser_view->GetTabContentsContainerView(),
            focus_manager->GetFocusedView());

  // Now press Ctrl+F again and focus should move to the Find box.
  ui_controls::SendKeyPressNotifyWhenDone(NULL, base::VKEY_F, true, false,
                                          false, new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  // See remark above on why we wait.
  ::Sleep(kActionDelayMs);
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  focused_view = focus_manager->GetFocusedView();
  ASSERT_TRUE(focused_view != NULL);
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, focused_view->GetID());
}
#endif  // defined(OS_WIN)

// Makes sure the focus is in the right location when opening the different
// types of tabs.
// TODO(estade): undisable this.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, TabInitialFocus) {
  // Open the history tab, focus should be on the tab contents.
  browser()->ShowHistoryTab();
  CheckViewHasFocus(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);

  // Open the new tab, focus should be on the location bar.
  browser()->NewTab();
  CheckViewHasFocus(VIEW_ID_LOCATION_BAR);

  // Open the download tab, focus should be on the tab contents.
  browser()->ShowDownloadsTab();
  CheckViewHasFocus(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);

  // Open about:blank, focus should be on the location bar.
  browser()->AddTabWithURL(GURL("about:blank"), GURL(), PageTransition::LINK,
                           true, -1, false, NULL);
  CheckViewHasFocus(VIEW_ID_LOCATION_BAR);
}

#if defined(OS_WIN)
// Tests that focus goes where expected when using reload.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusOnReload) {
  HTTPTestServer* server = StartHTTPServer();

  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(hwnd);
  ASSERT_TRUE(browser_view);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(hwnd);
  ASSERT_TRUE(focus_manager);

  // Open the new tab, reload.
  browser()->NewTab();
  browser()->Reload();
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  // Focus should stay on the location bar.
  EXPECT_EQ(browser_view->GetLocationBarView(),
            focus_manager->GetFocusedView());

  // Open a regular page, focus the location bar, reload.
  ui_test_utils::NavigateToURL(browser(), server->TestServerPageW(kSimplePage));
  browser_view->GetLocationBarView()->FocusLocation();
  EXPECT_EQ(browser_view->GetLocationBarView(),
            focus_manager->GetFocusedView());
  browser()->Reload();
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  // Focus should now be on the tab contents.
  EXPECT_EQ(browser_view->GetTabContentsContainerView(),
            focus_manager->GetFocusedView());
}

// Tests that focus goes where expected when using reload on a crashed tab.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusOnReloadCrashedTab) {
  HTTPTestServer* server = StartHTTPServer();

  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(hwnd);
  ASSERT_TRUE(browser_view);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(hwnd);
  ASSERT_TRUE(focus_manager);

  // Open a regular page, crash, reload.
  ui_test_utils::NavigateToURL(browser(), server->TestServerPageW(kSimplePage));
  ui_test_utils::CrashTab(browser()->GetSelectedTabContents());
  browser()->Reload();
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  // Focus should now be on the tab contents.
  EXPECT_EQ(browser_view->GetTabContentsContainerView(),
            focus_manager->GetFocusedView());
}
#endif  // defined(OS_WIN)
