// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

class ChromeNavigationBrowserTest : public InProcessBrowserTest {
 public:
  ChromeNavigationBrowserTest() {}
  ~ChromeNavigationBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Backgrounded renderer processes run at a lower priority, causing the
    // tests to take more time to complete. Disable backgrounding so that the
    // tests don't time out.
    command_line->AppendSwitch(switches::kDisableRendererBackgrounding);

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

// Tests that viewing frame source on a local file:// page with an iframe
// with a remote URL shows the correct tab title.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest, TestViewFrameSource) {
  // The local page file:// URL.
  GURL local_page_with_iframe_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("iframe.html")));

  // The non-file:// URL of the page to load in the iframe.
  GURL iframe_target_url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), local_page_with_iframe_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents->GetMainFrame(),
      base::StringPrintf("var iframe = document.getElementById('test');\n"
                         "iframe.setAttribute('src', '%s');\n",
                         iframe_target_url.spec().c_str())));
  observer.Wait();

  content::RenderFrameHost* frame =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
  ASSERT_TRUE(frame);
  ASSERT_NE(frame, web_contents->GetMainFrame());

  content::ContextMenuParams params;
  params.page_url = local_page_with_iframe_url;
  params.frame_url = frame->GetLastCommittedURL();
  params.frame_page_state = content::PageState::CreateFromURL(params.frame_url);
  TestRenderViewContextMenu menu(frame, params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE, 0);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_NE(new_web_contents, web_contents);
  WaitForLoadStop(new_web_contents);

  GURL view_frame_source_url(content::kViewSourceScheme + std::string(":") +
                             iframe_target_url.spec());
  EXPECT_EQ(url_formatter::FormatUrl(view_frame_source_url),
            new_web_contents->GetTitle());
}

class ChromeNavigationPortMappedBrowserTest : public InProcessBrowserTest {
 public:
  ChromeNavigationPortMappedBrowserTest() {}
  ~ChromeNavigationPortMappedBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->Start());

    // Use the command line parameter for the host resolver, so URLs without
    // explicit port numbers can be mapped under the hood to the port number
    // the |embedded_test_server| uses. It is required to test with potentially
    // malformed URLs.
    std::string port =
        base::IntToString(embedded_test_server()->host_port_pair().port());
    command_line->AppendSwitchASCII(
        "host-resolver-rules",
        "MAP * 127.0.0.1:" + port + ", EXCLUDE 127.0.0.1*");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeNavigationPortMappedBrowserTest);
};

// Test to verify that a malformed URL set as the virtual URL of a
// NavigationEntry will result in the navigation being dropped.
// See https://crbug.com/657720.
IN_PROC_BROWSER_TEST_F(ChromeNavigationPortMappedBrowserTest,
                       ContextMenuNavigationToInvalidUrl) {
  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  GURL new_tab_url(
      "www.foo.com::/server-redirect?http%3A%2F%2Fbar.com%2Ftitle2.html");

  // Navigate to an initial page, to ensure we have a committed document
  // from which to perform a context menu initiated navigation.
  ui_test_utils::NavigateToURL(browser(), initial_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // This corresponds to "Open link in new tab".
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::WebContextMenuData::MediaTypeNone;
  params.page_url = initial_url;
  params.link_url = new_tab_url;

  content::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

  TestRenderViewContextMenu menu(web_contents->GetMainFrame(), params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  // Wait for the new tab to be created and for loading to stop. The
  // navigation should not be allowed, therefore there should not be a last
  // committed URL in the new tab.
  tab_added_observer.Wait();
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(
          browser()->tab_strip_model()->count() - 1);
  WaitForLoadStop(new_web_contents);

  // If the test is unsuccessful, the return value from GetLastCommittedURL
  // will be the virtual URL for the created NavigationEntry.
  // Note: Before the bug was fixed, the URL was the new_tab_url with a scheme
  // prepended and one less ":" character after the host.
  EXPECT_EQ(GURL(), new_web_contents->GetLastCommittedURL());
}
