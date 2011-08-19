// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unload_uitest.h"

const char NOLISTENERS_HTML[] =
    "<html><head><title>nolisteners</title></head><body></body></html>";

const char UNLOAD_HTML[] =
    "<html><head><title>unload</title></head><body>"
    "<script>window.onunload=function(e){}</script></body></html>";

const char BEFORE_UNLOAD_HTML[] =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "</body></html>";

const char INNER_FRAME_WITH_FOCUS_HTML[] =
    "<html><head><title>innerframewithfocus</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "<iframe src=\"data:text/html,<html><head><script>window.onload="
    "function(){document.getElementById('box').focus()}</script>"
    "<body><input id='box'></input></body></html>\"></iframe>"
    "</body></html>";

const char TWO_SECOND_BEFORE_UNLOAD_HTML[] =
    "<html><head><title>twosecondbeforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
      "var start = new Date().getTime();"
      "while(new Date().getTime() - start < 2000){}"
      "return 'foo';"
    "}</script></body></html>";

const char INFINITE_UNLOAD_HTML[] =
    "<html><head><title>infiniteunload</title></head><body>"
    "<script>window.onunload=function(e){while(true){}}</script>"
    "</body></html>";

const char INFINITE_BEFORE_UNLOAD_HTML[] =
    "<html><head><title>infinitebeforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){while(true){}}</script>"
    "</body></html>";

const char INFINITE_UNLOAD_ALERT_HTML[] =
    "<html><head><title>infiniteunloadalert</title></head><body>"
    "<script>window.onunload=function(e){"
      "while(true){}"
      "alert('foo');"
    "}</script></body></html>";

const char INFINITE_BEFORE_UNLOAD_ALERT_HTML[] =
    "<html><head><title>infinitebeforeunloadalert</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
      "while(true){}"
      "alert('foo');"
    "}</script></body></html>";

const char TWO_SECOND_UNLOAD_ALERT_HTML[] =
    "<html><head><title>twosecondunloadalert</title></head><body>"
    "<script>window.onunload=function(e){"
      "var start = new Date().getTime();"
      "while(new Date().getTime() - start < 2000){}"
      "alert('foo');"
    "}</script></body></html>";

const char TWO_SECOND_BEFORE_UNLOAD_ALERT_HTML[] =
    "<html><head><title>twosecondbeforeunloadalert</title></head><body>"
    "<script>window.onbeforeunload=function(e){"
      "var start = new Date().getTime();"
      "while(new Date().getTime() - start < 2000){}"
      "alert('foo');"
    "}</script></body></html>";


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
#elif defined(OS_CHROMEOS)
// Flakily fails: http://crbug.com/86469
#define MAYBE_BrowserCloseInfiniteBeforeUnload \
    FLAKY_BrowserCloseInfiniteBeforeUnload
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

// TODO(ojan): Add tests for unload/beforeunload that have multiple tabs
// and multiple windows.
