// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_test_util.h"
#include "ui/base/events.h"
#include "ui/base/message_box_flags.h"

const std::string NOLISTENERS_HTML =
    "<html><head><title>nolisteners</title></head><body></body></html>";

const std::string UNLOAD_HTML =
    "<html><head><title>unload</title></head><body>"
    "<script>window.onunload=function(e){}</script></body></html>";

const std::string BEFORE_UNLOAD_HTML =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "</body></html>";

const std::string INNER_FRAME_WITH_FOCUS_HTML =
    "<html><head><title>innerframewithfocus</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "<iframe src=\"data:text/html,<html><head><script>window.onload="
    "function(){document.getElementById('box').focus()}</script>"
    "<body><input id='box'></input></body></html>\"></iframe>"
    "</body></html>";

const std::string TWO_SECOND_BEFORE_UNLOAD_HTML =
    "<html><head><title>twosecondbeforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
      "var start = new Date().getTime();"
      "while(new Date().getTime() - start < 2000){}"
      "return 'foo';"
    "}</script></body></html>";

const std::string INFINITE_UNLOAD_HTML =
    "<html><head><title>infiniteunload</title></head><body>"
    "<script>window.onunload=function(e){while(true){}}</script>"
    "</body></html>";

const std::string INFINITE_BEFORE_UNLOAD_HTML =
    "<html><head><title>infinitebeforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){while(true){}}</script>"
    "</body></html>";

const std::string INFINITE_UNLOAD_ALERT_HTML =
    "<html><head><title>infiniteunloadalert</title></head><body>"
    "<script>window.onunload=function(e){"
      "while(true){}"
      "alert('foo');"
    "}</script></body></html>";

const std::string INFINITE_BEFORE_UNLOAD_ALERT_HTML =
    "<html><head><title>infinitebeforeunloadalert</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
      "while(true){}"
      "alert('foo');"
    "}</script></body></html>";

const std::string TWO_SECOND_UNLOAD_ALERT_HTML =
    "<html><head><title>twosecondunloadalert</title></head><body>"
    "<script>window.onunload=function(e){"
      "var start = new Date().getTime();"
      "while(new Date().getTime() - start < 2000){}"
      "alert('foo');"
    "}</script></body></html>";

const std::string TWO_SECOND_BEFORE_UNLOAD_ALERT_HTML =
    "<html><head><title>twosecondbeforeunloadalert</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
      "var start = new Date().getTime();"
      "while(new Date().getTime() - start < 2000){}"
      "alert('foo');"
    "}</script></body></html>";

const std::string CLOSE_TAB_WHEN_OTHER_TAB_HAS_LISTENER =
    "<html><head><title>only_one_unload</title></head>"
    "<body onclick=\"window.open('data:text/html,"
    "<html><head><title>popup</title></head></body>')\" "
    "onbeforeunload='return;'>"
    "</body></html>";

class UnloadTest : public UITest {
 public:
  virtual void SetUp() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(),
        "BrowserCloseTabWhenOtherTabHasListener") == 0) {
      launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
    }

    UITest::SetUp();
  }

  void CheckTitle(const std::wstring& expected_title) {
    const int kCheckDelayMs = 100;
    for (int max_wait_time = TestTimeouts::action_max_timeout_ms();
         max_wait_time > 0; max_wait_time -= kCheckDelayMs) {
      if (expected_title == GetActiveTabTitle())
        break;
      base::PlatformThread::Sleep(kCheckDelayMs);
    }

    EXPECT_EQ(expected_title, GetActiveTabTitle());
  }

  void NavigateToDataURL(const std::string& html_content,
                         const std::wstring& expected_title) {
    NavigateToURL(GURL("data:text/html," + html_content));
    CheckTitle(expected_title);
  }

  void NavigateToNolistenersFileTwice() {
    NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                      FilePath(FILE_PATH_LITERAL("title2.html"))));
    CheckTitle(L"Title Of Awesomeness");
    NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                      FilePath(FILE_PATH_LITERAL("title2.html"))));
    CheckTitle(L"Title Of Awesomeness");
  }

  // Navigates to a URL asynchronously, then again synchronously. The first
  // load is purposely async to test the case where the user loads another
  // page without waiting for the first load to complete.
  void NavigateToNolistenersFileTwiceAsync() {
    NavigateToURLAsync(
        URLRequestMockHTTPJob::GetMockUrl(
            FilePath(FILE_PATH_LITERAL("title2.html"))));
    NavigateToURL(
        URLRequestMockHTTPJob::GetMockUrl(
            FilePath(FILE_PATH_LITERAL("title2.html"))));

    CheckTitle(L"Title Of Awesomeness");
  }

  void LoadUrlAndQuitBrowser(const std::string& html_content,
                             const std::wstring& expected_title = L"") {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser.get());
    NavigateToDataURL(html_content, expected_title);
    bool application_closed = false;
    EXPECT_TRUE(CloseBrowser(browser.get(), &application_closed));
  }

  void ClickModalDialogButton(ui::MessageBoxFlags::DialogButton button) {
    bool modal_dialog_showing = false;
    ui::MessageBoxFlags::DialogButton available_buttons;
    EXPECT_TRUE(automation()->WaitForAppModalDialog());
    EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
        &available_buttons));
    ASSERT_TRUE(modal_dialog_showing);
    EXPECT_TRUE((button & available_buttons) != 0);
    EXPECT_TRUE(automation()->ClickAppModalDialogButton(button));
  }
};

// Navigate to a page with an infinite unload handler.
// Then two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
//
// This test is flaky on the valgrind UI bots. http://crbug.com/39057
TEST_F(UnloadTest, DISABLED_CrossSiteInfiniteUnloadAsync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_UNLOAD_HTML, L"infiniteunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwiceAsync();
  ASSERT_TRUE(IsBrowserRunning());
}

// Navigate to a page with an infinite unload handler.
// Then two sync crosssite requests to ensure
// we correctly nav to each one.
TEST_F(UnloadTest, CrossSiteInfiniteUnloadSync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_UNLOAD_HTML, L"infiniteunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwice();
  ASSERT_TRUE(IsBrowserRunning());
}

// TODO(creis): This test is currently failing intermittently on Linux and
// consistently on Mac and Vista.  http://crbug.com/38427
#if defined(OS_MACOSX)
#define MAYBE_CrossSiteInfiniteUnloadAsyncInputEvent \
    DISABLED_CrossSiteInfiniteUnloadAsyncInputEvent
#elif defined(OS_WIN)
#define MAYBE_CrossSiteInfiniteUnloadAsyncInputEvent \
    DISABLED_CrossSiteInfiniteUnloadAsyncInputEvent
#else
// Flaky on Linux.  http://crbug.com/38427
#define MAYBE_CrossSiteInfiniteUnloadAsyncInputEvent \
    FLAKY_CrossSiteInfiniteUnloadAsyncInputEvent
#endif

// Navigate to a page with an infinite unload handler.
// Then an async crosssite request followed by an input event to ensure that
// the short unload timeout (not the long input event timeout) is used.
// See crbug.com/11007.
TEST_F(UnloadTest, MAYBE_CrossSiteInfiniteUnloadAsyncInputEvent) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_UNLOAD_HTML, L"infiniteunload");

  // Navigate to a new URL asynchronously.
  NavigateToURLAsync(
      URLRequestMockHTTPJob::GetMockUrl(
          FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Now send an input event while we're stalled on the unload handler.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());
  gfx::Rect bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_0, &bounds, false));
  ASSERT_TRUE(browser->SimulateDrag(bounds.CenterPoint(), bounds.CenterPoint(),
                                    ui::EF_LEFT_BUTTON_DOWN, false));

  // The title should update before the timeout in CheckTitle.
  CheckTitle(L"Title Of Awesomeness");
  ASSERT_TRUE(IsBrowserRunning());
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
// This test is flaky on the valgrind UI bots. http://crbug.com/39057
TEST_F(UnloadTest, FLAKY_CrossSiteInfiniteBeforeUnloadAsync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_BEFORE_UNLOAD_HTML, L"infinitebeforeunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwiceAsync();
  ASSERT_TRUE(IsBrowserRunning());
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two sync crosssite requests to ensure
// we correctly nav to each one.
TEST_F(UnloadTest, CrossSiteInfiniteBeforeUnloadSync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_BEFORE_UNLOAD_HTML, L"infinitebeforeunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwice();
  ASSERT_TRUE(IsBrowserRunning());
}

// Tests closing the browser on a page with no unload listeners registered.
TEST_F(UnloadTest, BrowserCloseNoUnloadListeners) {
  LoadUrlAndQuitBrowser(NOLISTENERS_HTML, L"nolisteners");
}

// Tests closing the browser on a page with an unload listener registered.
// Test marked as flaky in http://crbug.com/51698
TEST_F(UnloadTest, FLAKY_BrowserCloseUnload) {
  LoadUrlAndQuitBrowser(UNLOAD_HTML, L"unload");
}

// Tests closing the browser with a beforeunload handler and clicking
// OK in the beforeunload confirm dialog.
TEST_F(UnloadTest, BrowserCloseBeforeUnloadOK) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  NavigateToDataURL(BEFORE_UNLOAD_HTML, L"beforeunload");

  CloseBrowserAsync(browser.get());
  ClickModalDialogButton(ui::MessageBoxFlags::DIALOGBUTTON_OK);

  int exit_code = -1;
  ASSERT_TRUE(launcher_->WaitForBrowserProcessToQuit(
                  TestTimeouts::action_max_timeout_ms(), &exit_code));
  EXPECT_EQ(0, exit_code);  // Expect a clean shutown.
}

// Tests closing the browser with a beforeunload handler and clicking
// CANCEL in the beforeunload confirm dialog.
TEST_F(UnloadTest, BrowserCloseBeforeUnloadCancel) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  NavigateToDataURL(BEFORE_UNLOAD_HTML, L"beforeunload");

  CloseBrowserAsync(browser.get());
  ClickModalDialogButton(ui::MessageBoxFlags::DIALOGBUTTON_CANCEL);

  // There's no real graceful way to wait for something _not_ to happen, so
  // we just wait a short period.
  base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());
  ASSERT_TRUE(IsBrowserRunning());

  CloseBrowserAsync(browser.get());
  ClickModalDialogButton(ui::MessageBoxFlags::DIALOGBUTTON_OK);

  int exit_code = -1;
  ASSERT_TRUE(launcher_->WaitForBrowserProcessToQuit(
                  TestTimeouts::action_max_timeout_ms(), &exit_code));
  EXPECT_EQ(0, exit_code);  // Expect a clean shutdown.
}

#if defined(OS_LINUX)
// Fails sometimes on Linux valgrind. http://crbug.com/45675
#define MAYBE_BrowserCloseWithInnerFocusedFrame \
    FLAKY_BrowserCloseWithInnerFocusedFrame
#else
#define MAYBE_BrowserCloseWithInnerFocusedFrame \
    BrowserCloseWithInnerFocusedFrame
#endif

// Tests closing the browser and clicking OK in the beforeunload confirm dialog
// if an inner frame has the focus.  See crbug.com/32615.
TEST_F(UnloadTest, MAYBE_BrowserCloseWithInnerFocusedFrame) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  NavigateToDataURL(INNER_FRAME_WITH_FOCUS_HTML, L"innerframewithfocus");

  CloseBrowserAsync(browser.get());
  ClickModalDialogButton(ui::MessageBoxFlags::DIALOGBUTTON_OK);

  int exit_code = -1;
  ASSERT_TRUE(launcher_->WaitForBrowserProcessToQuit(
                  TestTimeouts::action_max_timeout_ms(), &exit_code));
  EXPECT_EQ(0, exit_code);  // Expect a clean shutdown.
}

// Tests closing the browser with a beforeunload handler that takes
// two seconds to run.
TEST_F(UnloadTest, BrowserCloseTwoSecondBeforeUnload) {
  LoadUrlAndQuitBrowser(TWO_SECOND_BEFORE_UNLOAD_HTML,
                        L"twosecondbeforeunload");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an infinite loop.
TEST_F(UnloadTest, BrowserCloseInfiniteUnload) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_UNLOAD_HTML, L"infiniteunload");
}

#if defined(OS_WIN)
// Flakily fails, times out: http://crbug.com/78803
#define MAYBE_BrowserCloseInfiniteBeforeUnload \
    DISABLED_BrowserCloseInfiniteBeforeUnload
#else
#define MAYBE_BrowserCloseInfiniteBeforeUnload BrowserCloseInfiniteBeforeUnload
#endif
// Tests closing the browser with a beforeunload handler that hangs.
TEST_F(UnloadTest, MAYBE_BrowserCloseInfiniteBeforeUnload) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_BEFORE_UNLOAD_HTML, L"infinitebeforeunload");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an infinite loop followed by an alert.
TEST_F(UnloadTest, BrowserCloseInfiniteUnloadAlert) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_UNLOAD_ALERT_HTML, L"infiniteunloadalert");
}

#if defined(OS_WIN)
// Flakily fails, times out: http://crbug.com/78803
#define MAYBE_BrowserCloseInfiniteBeforeUnloadAlert \
    DISABLED_BrowserCloseInfiniteBeforeUnloadAlert
#else
#define MAYBE_BrowserCloseInfiniteBeforeUnloadAlert \
    BrowserCloseInfiniteBeforeUnloadAlert
#endif
// Tests closing the browser with a beforeunload handler that hangs then
// pops up an alert.
TEST_F(UnloadTest, MAYBE_BrowserCloseInfiniteBeforeUnloadAlert) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_BEFORE_UNLOAD_ALERT_HTML,
                        L"infinitebeforeunloadalert");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an 2 second long loop followed by an alert.
TEST_F(UnloadTest, BrowserCloseTwoSecondUnloadAlert) {
  LoadUrlAndQuitBrowser(TWO_SECOND_UNLOAD_ALERT_HTML, L"twosecondunloadalert");
}

// Tests closing the browser with a beforeunload handler that takes
// two seconds to run then pops up an alert.
TEST_F(UnloadTest, BrowserCloseTwoSecondBeforeUnloadAlert) {
  LoadUrlAndQuitBrowser(TWO_SECOND_BEFORE_UNLOAD_ALERT_HTML,
                        L"twosecondbeforeunloadalert");
}

#if defined(OS_MACOSX)
// http://crbug.com/45162
#define MAYBE_BrowserCloseTabWhenOtherTabHasListener \
    DISABLED_BrowserCloseTabWhenOtherTabHasListener
#elif defined(OS_WIN)
// http://crbug.com/45281
#define MAYBE_BrowserCloseTabWhenOtherTabHasListener \
    DISABLED_BrowserCloseTabWhenOtherTabHasListener
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

// TODO(ojan): Add tests for unload/beforeunload that have multiple tabs
// and multiple windows.
