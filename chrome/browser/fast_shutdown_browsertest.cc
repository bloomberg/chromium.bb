// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {

void GetCookiesCallback(std::string* cookies_out,
                        base::WaitableEvent* event,
                        const std::string& cookies) {
  *cookies_out = cookies;
  event->Signal();
}

void GetCookiesOnIOThread(const GURL& url,
                          net::URLRequestContextGetter* context_getter,
                          base::WaitableEvent* event,
                          std::string* cookies) {
  net::CookieStore* cookie_store =
      context_getter->GetURLRequestContext()->cookie_store();
  cookie_store->GetCookiesWithOptionsAsync(
      url, net::CookieOptions(),
      base::Bind(&GetCookiesCallback,
                 base::Unretained(cookies), base::Unretained(event)));
}

}  // namespace

class FastShutdown : public InProcessBrowserTest {
 protected:
  FastShutdown() {
  }

  std::string GetCookies(const GURL& url) {
    std::string cookies;
    base::WaitableEvent event(true, false);
    net::URLRequestContextGetter* context_getter =
        browser()->profile()->GetRequestContext();

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&GetCookiesOnIOThread, url,
                   make_scoped_refptr(context_getter), &event, &cookies));
    event.Wait();
    return cookies;
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kDisablePopupBlocking);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FastShutdown);
};

// This tests for a previous error where uninstalling an onbeforeunload handler
// would enable fast shutdown even if an onunload handler still existed.
// Flaky on all platforms, http://crbug.com/89173
#if !defined(OS_CHROMEOS)  // ChromeOS opens tabs instead of windows for popups.
IN_PROC_BROWSER_TEST_F(FastShutdown, SlowTermination) {
  // Need to run these tests on http:// since we only allow cookies on that (and
  // https obviously).
  ASSERT_TRUE(test_server()->Start());
  // This page has an unload handler.
  GURL url = test_server()->GetURL("files/fast_shutdown/on_unloader.html");
  EXPECT_EQ("", GetCookies(url));

  ui_test_utils::WindowedNotificationObserver window_observer(
        chrome::NOTIFICATION_BROWSER_WINDOW_READY,
        content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_NONE);
  window_observer.Wait();

  // Close the new window, removing the one and only beforeunload handler.
  ASSERT_EQ(2u, BrowserList::size());
  BrowserList::const_iterator i = BrowserList::begin();
  ++i;
  chrome::CloseWindow(*i);

  // Need to wait for the renderer process to shutdown to ensure that we got the
  // set cookies IPC.
  ui_test_utils::WindowedNotificationObserver renderer_shutdown_observer(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::NotificationService::AllSources());
  // Close the tab. This should launch the unload handler, which sets a cookie
  // that's stored to disk.
  chrome::CloseTab(browser());
  renderer_shutdown_observer.Wait();

  
  EXPECT_EQ("unloaded=ohyeah", GetCookies(url));
}
#endif
