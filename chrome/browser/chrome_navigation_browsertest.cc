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

  // Runs a nested run loop and blocks until the full load has
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

// Tests that verify that ctrl-click results 1) open up in a new renderer
// process (https://crbug.com/23815) and 2) are in a new browsing instance (e.g.
// cannot find the opener's window by name - https://crbug.com/658386).
class CtrlClickShouldEndUpInNewProcessTest
    : public ChromeNavigationBrowserTest {
 protected:
  // Simulates ctrl-clicking an anchor with the given id in |main_contents|.
  // Verifies that the new contents are in a separate process and separate
  // browsing instance from |main_contents|.  Returns contents of the newly
  // opened tab.
  content::WebContents* SimulateCtrlClick(content::WebContents* main_contents,
                                          const char* id_of_anchor_to_click) {
    // Ctrl-click the anchor/link in the page.
    content::WebContents* new_contents = nullptr;
    {
      content::WebContentsAddedObserver new_tab_observer;
#if defined(OS_MACOSX)
      const char* new_tab_click_script_template =
          "simulateClick(\"%s\", { metaKey: true });";
#else
      const char* new_tab_click_script_template =
          "simulateClick(\"%s\", { ctrlKey: true });";
#endif
      std::string new_tab_click_script = base::StringPrintf(
          new_tab_click_script_template, id_of_anchor_to_click);
      EXPECT_TRUE(ExecuteScript(main_contents, new_tab_click_script));

      // Wait for a new tab to appear (the whole point of this test).
      new_contents = new_tab_observer.GetWebContents();
    }

    // Verify that the new tab has the right contents and is in the right, new
    // place in the tab strip.
    EXPECT_TRUE(WaitForLoadStop(new_contents));
    int last_tab_index = browser()->tab_strip_model()->count() - 1;
    EXPECT_LT(1, browser()->tab_strip_model()->count());  // More than 1 tab?
    EXPECT_EQ(new_contents,
              browser()->tab_strip_model()->GetWebContentsAt(last_tab_index));
    GURL expected_url(embedded_test_server()->GetURL("/title1.html"));
    EXPECT_EQ(expected_url, new_contents->GetLastCommittedURL());

    // Verify that the new tab is in a different process, SiteInstance and
    // BrowsingInstance from the old contents.
    EXPECT_NE(main_contents->GetMainFrame()->GetProcess(),
              new_contents->GetMainFrame()->GetProcess());
    EXPECT_NE(main_contents->GetMainFrame()->GetSiteInstance(),
              new_contents->GetMainFrame()->GetSiteInstance());
    EXPECT_FALSE(main_contents->GetSiteInstance()->IsRelatedSiteInstance(
        new_contents->GetSiteInstance()));

    // Verify that the new BrowsingInstance can't see windows from the old one.
    {
      // Double-check that main_contents has expected window.name set.
      // This is a sanity check of test setup; this is not a product test.
      std::string name_of_main_contents_window;
      EXPECT_TRUE(ExecuteScriptAndExtractString(
          main_contents, "window.domAutomationController.send(window.name)",
          &name_of_main_contents_window));
      EXPECT_EQ("main_contents", name_of_main_contents_window);

      // Verify that the new contents doesn't have a window.opener set.
      bool window_opener_cast_to_bool = true;
      EXPECT_TRUE(ExecuteScriptAndExtractBool(
          new_contents, "window.domAutomationController.send(!!window.opener)",
          &window_opener_cast_to_bool));
      EXPECT_FALSE(window_opener_cast_to_bool);

      // Verify that the new contents cannot find the old contents via
      // window.open. (i.e. window.open should open a new window, rather than
      // returning a reference to main_contents / old window).
      std::string location_of_opened_window;
      EXPECT_TRUE(ExecuteScriptAndExtractString(
          new_contents,
          "w = window.open('', 'main_contents');"
          "window.domAutomationController.send(w.location.href);",
          &location_of_opened_window));
      EXPECT_EQ(url::kAboutBlankURL, location_of_opened_window);
    }

    return new_contents;
  }

  void TestCtrlClick(const char* id_of_anchor_to_click) {
    // Navigate to the test page.
    GURL main_url(embedded_test_server()->GetURL(
        "/frame_tree/anchor_to_same_site_location.html"));
    ui_test_utils::NavigateToURL(browser(), main_url);

    // Verify that there is only 1 active tab (with the right contents
    // committed).
    EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
    content::WebContents* main_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_EQ(main_url, main_contents->GetLastCommittedURL());

    // Test what happens after ctrl-click.  SimulateCtrlClick will verify
    // that |new_contents1| is in a separate process and browsing instance
    // from |main_contents|.
    content::WebContents* new_contents1 =
        SimulateCtrlClick(main_contents, id_of_anchor_to_click);

    // Test that each subsequent ctrl-click also gets a new process.
    content::WebContents* new_contents2 =
        SimulateCtrlClick(main_contents, id_of_anchor_to_click);
    EXPECT_NE(new_contents1->GetMainFrame()->GetProcess(),
              new_contents2->GetMainFrame()->GetProcess());
    EXPECT_NE(new_contents1->GetMainFrame()->GetSiteInstance(),
              new_contents2->GetMainFrame()->GetSiteInstance());
    EXPECT_FALSE(new_contents1->GetSiteInstance()->IsRelatedSiteInstance(
        new_contents2->GetSiteInstance()));
  }
};

IN_PROC_BROWSER_TEST_F(CtrlClickShouldEndUpInNewProcessTest, NoTarget) {
  TestCtrlClick("test-anchor-no-target");
}

IN_PROC_BROWSER_TEST_F(CtrlClickShouldEndUpInNewProcessTest, BlankTarget) {
  TestCtrlClick("test-anchor-with-blank-target");
}

IN_PROC_BROWSER_TEST_F(CtrlClickShouldEndUpInNewProcessTest, SubframeTarget) {
  TestCtrlClick("test-anchor-with-subframe-target");
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
  params.media_type = blink::WebContextMenuData::kMediaTypeNone;
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

// A test performing two simultaneous navigations, to ensure code in chrome/,
// such as tab helpers, can handle those cases.
// This test starts a browser-initiated cross-process navigation, which is
// delayed. At the same time, the renderer does a synchronous navigation
// through pushState, which will create a separate navigation and associated
// NavigationHandle. Afterwards, the original cross-process navigation is
// resumed and confirmed to properly commit.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       SlowCrossProcessNavigationWithPushState) {
  const GURL kURL1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kPushStateURL =
      embedded_test_server()->GetURL("/title1.html#fragment");
  const GURL kURL2 = embedded_test_server()->GetURL("/title2.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to the initial page.
  ui_test_utils::NavigateToURL(browser(), kURL1);

  // Start navigating to the second page.
  content::TestNavigationManager manager(web_contents, kURL2);
  content::NavigationHandleCommitObserver navigation_observer(web_contents,
                                                              kURL2);
  web_contents->GetController().LoadURL(
      kURL2, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(manager.WaitForRequestStart());

  // The current page does a PushState.
  content::NavigationHandleCommitObserver push_state_observer(web_contents,
                                                              kPushStateURL);
  std::string push_state =
      "history.pushState({}, \"title 1\", \"" + kPushStateURL.spec() + "\");";
  EXPECT_TRUE(ExecuteScript(web_contents, push_state));
  content::NavigationEntry* last_committed =
      web_contents->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(last_committed);
  EXPECT_EQ(kPushStateURL, last_committed->GetURL());

  EXPECT_TRUE(push_state_observer.has_committed());
  EXPECT_TRUE(push_state_observer.was_same_document());
  EXPECT_TRUE(push_state_observer.was_renderer_initiated());

  // Let the navigation finish. It should commit successfully.
  manager.WaitForNavigationFinished();
  last_committed = web_contents->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(last_committed);
  EXPECT_EQ(kURL2, last_committed->GetURL());

  EXPECT_TRUE(navigation_observer.has_committed());
  EXPECT_FALSE(navigation_observer.was_same_document());
  EXPECT_FALSE(navigation_observer.was_renderer_initiated());
}
