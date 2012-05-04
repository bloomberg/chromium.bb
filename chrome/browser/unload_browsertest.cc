// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_POSIX)
#include <signal.h>
#endif

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "net/url_request/url_request_test_util.h"

using base::TimeDelta;
using content::BrowserThread;

const std::string NOLISTENERS_HTML =
    "<html><head><title>nolisteners</title></head><body></body></html>";

const std::string UNLOAD_HTML =
    "<html><head><title>unload</title></head><body>"
    "<script>window.onunload=function(e){}</script></body></html>";

const std::string BEFORE_UNLOAD_HTML =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
    "setTimeout('document.title=\"cancelled\"', 0);return 'foo'}</script>"
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

class UnloadTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(),
        "BrowserCloseTabWhenOtherTabHasListener") == 0) {
      command_line->AppendSwitch(switches::kDisablePopupBlocking);
    } else if (strcmp(test_info->name(), "BrowserTerminateBeforeUnload") == 0) {
#if defined(OS_POSIX)
      DisableSIGTERMHandling();
#endif
    }
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  void CheckTitle(const char* expected_title) {
    string16 expected = ASCIIToUTF16(expected_title);
    EXPECT_EQ(expected, browser()->GetSelectedWebContents()->GetTitle());
  }

  void NavigateToDataURL(const std::string& html_content,
                         const char* expected_title) {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("data:text/html," + html_content));
    CheckTitle(expected_title);
  }

  void NavigateToNolistenersFileTwice() {
    GURL url(URLRequestMockHTTPJob::GetMockUrl(
        FilePath(FILE_PATH_LITERAL("title2.html"))));
    ui_test_utils::NavigateToURL(browser(), url);
    CheckTitle("Title Of Awesomeness");
    ui_test_utils::NavigateToURL(browser(), url);
    CheckTitle("Title Of Awesomeness");
  }

  // Navigates to a URL asynchronously, then again synchronously. The first
  // load is purposely async to test the case where the user loads another
  // page without waiting for the first load to complete.
  void NavigateToNolistenersFileTwiceAsync() {
    GURL url(URLRequestMockHTTPJob::GetMockUrl(
        FilePath(FILE_PATH_LITERAL("title2.html"))));
    ui_test_utils::NavigateToURLWithDisposition(browser(), url, CURRENT_TAB, 0);
    ui_test_utils::NavigateToURL(browser(), url);
    CheckTitle("Title Of Awesomeness");
  }

  void LoadUrlAndQuitBrowser(const std::string& html_content,
                             const char* expected_title) {
    NavigateToDataURL(html_content, expected_title);
    ui_test_utils::WindowedNotificationObserver window_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
    browser()->CloseWindow();
    window_observer.Wait();
  }

  // If |accept| is true, simulates user clicking OK, otherwise simulates
  // clicking Cancel.
  void ClickModalDialogButton(bool accept) {
    AppModalDialog* dialog = ui_test_utils::WaitForAppModalDialog();
    ASSERT_TRUE(dialog->IsJavaScriptModalDialog());
    JavaScriptAppModalDialog* js_dialog =
        static_cast<JavaScriptAppModalDialog*>(dialog);
    if (accept)
      js_dialog->native_dialog()->AcceptAppModalDialog();
    else
      js_dialog->native_dialog()->CancelAppModalDialog();
  }
};

// Navigate to a page with an infinite unload handler.
// Then two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
//
// This test is flaky on the valgrind UI bots. http://crbug.com/39057
IN_PROC_BROWSER_TEST_F(UnloadTest, CrossSiteInfiniteUnloadAsync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_UNLOAD_HTML, "infiniteunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwiceAsync();
}

// Navigate to a page with an infinite unload handler.
// Then two sync crosssite requests to ensure
// we correctly nav to each one.
IN_PROC_BROWSER_TEST_F(UnloadTest, CrossSiteInfiniteUnloadSync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_UNLOAD_HTML, "infiniteunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwice();
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
// This test is flaky on the valgrind UI bots. http://crbug.com/39057
IN_PROC_BROWSER_TEST_F(UnloadTest, CrossSiteInfiniteBeforeUnloadAsync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_BEFORE_UNLOAD_HTML, "infinitebeforeunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwiceAsync();
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two sync crosssite requests to ensure
// we correctly nav to each one.
// If this flakes, reopen bug http://crbug.com/86469.
IN_PROC_BROWSER_TEST_F(UnloadTest, CrossSiteInfiniteBeforeUnloadSync) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  NavigateToDataURL(INFINITE_BEFORE_UNLOAD_HTML, "infinitebeforeunload");
  // Must navigate to a non-data URL to trigger cross-site codepath.
  NavigateToNolistenersFileTwice();
}

// Tests closing the browser on a page with no unload listeners registered.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseNoUnloadListeners) {
  LoadUrlAndQuitBrowser(NOLISTENERS_HTML, "nolisteners");
}

// Tests closing the browser on a page with an unload listener registered.
// Test marked as flaky in http://crbug.com/51698
IN_PROC_BROWSER_TEST_F(UnloadTest, DISABLED_BrowserCloseUnload) {
  LoadUrlAndQuitBrowser(UNLOAD_HTML, "unload");
}

// Tests closing the browser with a beforeunload handler and clicking
// OK in the beforeunload confirm dialog.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseBeforeUnloadOK) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");

  ui_test_utils::WindowedNotificationObserver window_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
  browser()->CloseWindow();
  ClickModalDialogButton(true);
  window_observer.Wait();
}

// Tests closing the browser with a beforeunload handler and clicking
// CANCEL in the beforeunload confirm dialog.
// If this test flakes, reopen http://crbug.com/123110
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseBeforeUnloadCancel) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  browser()->CloseWindow();

  // We wait for the title to change after cancelling the popup to ensure that
  // in-flight IPCs from the renderer reach the browser. Otherwise the browser
  // won't put up the beforeunload dialog because it's waiting for an ack from
  // the renderer.
  string16 expected_title = ASCIIToUTF16("cancelled");
  ui_test_utils::TitleWatcher title_watcher(
      browser()->GetSelectedWebContents(), expected_title);
  ClickModalDialogButton(false);
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  ui_test_utils::WindowedNotificationObserver window_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
  browser()->CloseWindow();
  ClickModalDialogButton(true);
  window_observer.Wait();
}

// Tests terminating the browser with a beforeunload handler.
// Currently only ChromeOS shuts down gracefully.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserTerminateBeforeUnload) {
  NavigateToDataURL(BEFORE_UNLOAD_HTML, "beforeunload");
  EXPECT_EQ(kill(base::GetCurrentProcessHandle(), SIGTERM), 0);
}
#endif

// Tests closing the browser and clicking OK in the beforeunload confirm dialog
// if an inner frame has the focus.
// If this flakes, use http://crbug.com/32615 and http://crbug.com/45675
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseWithInnerFocusedFrame) {
  NavigateToDataURL(INNER_FRAME_WITH_FOCUS_HTML, "innerframewithfocus");

  ui_test_utils::WindowedNotificationObserver window_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
  browser()->CloseWindow();
  ClickModalDialogButton(true);
  window_observer.Wait();
}

// Tests closing the browser with a beforeunload handler that takes
// two seconds to run.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseTwoSecondBeforeUnload) {
  LoadUrlAndQuitBrowser(TWO_SECOND_BEFORE_UNLOAD_HTML,
                        "twosecondbeforeunload");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an infinite loop.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseInfiniteUnload) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_UNLOAD_HTML, "infiniteunload");
}

// Tests closing the browser with a beforeunload handler that hangs.
// If this flakes, use http://crbug.com/78803 and http://crbug.com/86469
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseInfiniteBeforeUnload) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_BEFORE_UNLOAD_HTML, "infinitebeforeunload");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an infinite loop followed by an alert.
// If this flakes, use http://crbug.com/86469
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseInfiniteUnloadAlert) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_UNLOAD_ALERT_HTML, "infiniteunloadalert");
}

// Tests closing the browser with a beforeunload handler that hangs then
// pops up an alert.
// If this flakes, use http://crbug.com/78803.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseInfiniteBeforeUnloadAlert) {
  // Tests makes no sense in single-process mode since the renderer is hung.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  LoadUrlAndQuitBrowser(INFINITE_BEFORE_UNLOAD_ALERT_HTML,
                        "infinitebeforeunloadalert");
}

// Tests closing the browser on a page with an unload listener registered where
// the unload handler has an 2 second long loop followed by an alert.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseTwoSecondUnloadAlert) {
  LoadUrlAndQuitBrowser(TWO_SECOND_UNLOAD_ALERT_HTML, "twosecondunloadalert");
}

// Tests closing the browser with a beforeunload handler that takes
// two seconds to run then pops up an alert.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseTwoSecondBeforeUnloadAlert) {
  LoadUrlAndQuitBrowser(TWO_SECOND_BEFORE_UNLOAD_ALERT_HTML,
                        "twosecondbeforeunloadalert");
}

// Tests that if there's a renderer process with two tabs, one of which has an
// unload handler, and the other doesn't, the tab that doesn't have an unload
// handler can be closed.
// If this flakes, see http://crbug.com/45162, http://crbug.com/45281 and
// http://crbug.com/86769.
IN_PROC_BROWSER_TEST_F(UnloadTest, BrowserCloseTabWhenOtherTabHasListener) {
  NavigateToDataURL(CLOSE_TAB_WHEN_OTHER_TAB_HAS_LISTENER, "only_one_unload");

  // Simulate a click to force user_gesture to true; if we don't, the resulting
  // popup will be constrained, which isn't what we want to test.

  ui_test_utils::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
  ui_test_utils::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ui_test_utils::SimulateMouseClick(browser()->GetSelectedWebContents());
  observer.Wait();
  load_stop_observer.Wait();
  CheckTitle("popup");

  ui_test_utils::WindowedNotificationObserver tab_close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::NotificationService::AllSources());
  browser()->CloseTab();
  tab_close_observer.Wait();

  CheckTitle("only_one_unload");
}

// TODO(ojan): Add tests for unload/beforeunload that have multiple tabs
// and multiple windows.
