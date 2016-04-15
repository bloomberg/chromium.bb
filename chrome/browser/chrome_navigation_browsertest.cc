// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

class ChromeNavigationBrowserTest : public InProcessBrowserTest {
 public:
  ChromeNavigationBrowserTest() {}
  ~ChromeNavigationBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void StartServerWithExpiredCert() {
    expired_https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    expired_https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    expired_https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    ASSERT_TRUE(expired_https_server_->Start());
  }

  net::EmbeddedTestServer* expired_https_server() {
    return expired_https_server_.get();
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> expired_https_server_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNavigationBrowserTest);
};

// Helper class to track and allow waiting for navigation start events.
class DidStartNavigationObserver : public content::WebContentsObserver {
 public:
  explicit DidStartNavigationObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        message_loop_runner_(new content::MessageLoopRunner) {}
  ~DidStartNavigationObserver() override {}

  // Runs a nested message loop and blocks until the full load has
  // completed.
  void Wait() { message_loop_runner_->Run(); }

 private:
  // WebContentsObserver
  void DidStartNavigation(content::NavigationHandle* handle) override {
    if (message_loop_runner_->loop_running())
      message_loop_runner_->Quit();
  }

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(DidStartNavigationObserver);
};

// Test to verify that navigations are not deleting the transient
// NavigationEntry when showing an interstitial page and the old renderer
// process is trying to navigate. See https://crbug.com/600046.
IN_PROC_BROWSER_TEST_F(
    ChromeNavigationBrowserTest,
    TransientEntryPreservedOnMultipleNavigationsDuringInterstitial) {
  StartServerWithExpiredCert();

  GURL setup_url =
      embedded_test_server()->GetURL("/window_open_and_navigate.html");
  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  GURL error_url(expired_https_server()->GetURL("/ssl/blank_page.html"));

  ui_test_utils::NavigateToURL(browser(), setup_url);
  content::WebContents* main_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Call the JavaScript method in the test page, which opens a new window
  // and stores a handle to it.
  content::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  EXPECT_TRUE(content::ExecuteScript(main_web_contents, "openWin();"));
  tab_added_observer.Wait();
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate the opened window to a page that will successfully commit and
  // create a NavigationEntry.
  {
    content::TestNavigationObserver observer(new_web_contents);
    EXPECT_TRUE(content::ExecuteScript(
        main_web_contents, "navigate('" + initial_url.spec() + "');"));
    observer.Wait();
    EXPECT_EQ(initial_url, new_web_contents->GetLastCommittedURL());
  }

  // Navigate the opened window to a page which will trigger an
  // interstitial.
  {
    content::TestNavigationObserver observer(new_web_contents);
    EXPECT_TRUE(content::ExecuteScript(
        main_web_contents, "navigate('" + error_url.spec() + "');"));
    observer.Wait();
    EXPECT_EQ(initial_url, new_web_contents->GetLastCommittedURL());
    EXPECT_EQ(error_url, new_web_contents->GetVisibleURL());
  }

  // Navigate again the opened window to the same page. It should not cause
  // WebContents::GetVisibleURL to return the last committed one.
  {
    DidStartNavigationObserver nav_observer(new_web_contents);
    EXPECT_TRUE(content::ExecuteScript(
        main_web_contents, "navigate('" + error_url.spec() + "');"));
    nav_observer.Wait();
    EXPECT_EQ(error_url, new_web_contents->GetVisibleURL());
    EXPECT_TRUE(new_web_contents->GetController().GetTransientEntry());
    EXPECT_FALSE(new_web_contents->IsLoading());
  }
}
