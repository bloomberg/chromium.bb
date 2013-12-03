// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class SitePerProcessWebContentsObserver: public WebContentsObserver {
 public:
  explicit SitePerProcessWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        navigation_succeeded_(true) {}
  virtual ~SitePerProcessWebContentsObserver() {}

  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      const base::string16& frame_unique_name,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      RenderViewHost* render_view_host) OVERRIDE {
    navigation_url_ = validated_url;
    navigation_succeeded_ = false;
  }

  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      const base::string16& frame_unique_name,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type,
      RenderViewHost* render_view_host) OVERRIDE{
    navigation_url_ = url;
    navigation_succeeded_ = true;
  }

  const GURL& navigation_url() const {
    return navigation_url_;
  }

  int navigation_succeeded() const { return navigation_succeeded_; }

 private:
  GURL navigation_url_;
  bool navigation_succeeded_;

  DISALLOW_COPY_AND_ASSIGN(SitePerProcessWebContentsObserver);
};

class RedirectNotificationObserver : public NotificationObserver {
 public:
  // Register to listen for notifications of the given type from either a
  // specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  RedirectNotificationObserver(int notification_type,
                               const NotificationSource& source);
  virtual ~RedirectNotificationObserver();

  // Wait until the specified notification occurs.  If the notification was
  // emitted between the construction of this object and this call then it
  // returns immediately.
  void Wait();

  // Returns NotificationService::AllSources() if we haven't observed a
  // notification yet.
  const NotificationSource& source() const {
    return source_;
  }

  const NotificationDetails& details() const {
    return details_;
  }

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  bool seen_;
  bool seen_twice_;
  bool running_;
  NotificationRegistrar registrar_;

  NotificationSource source_;
  NotificationDetails details_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(RedirectNotificationObserver);
};

RedirectNotificationObserver::RedirectNotificationObserver(
    int notification_type,
    const NotificationSource& source)
    : seen_(false),
      running_(false),
      source_(NotificationService::AllSources()) {
  registrar_.Add(this, notification_type, source);
}

RedirectNotificationObserver::~RedirectNotificationObserver() {}

void RedirectNotificationObserver::Wait() {
  if (seen_ && seen_twice_)
    return;

  running_ = true;
  message_loop_runner_ = new MessageLoopRunner;
  message_loop_runner_->Run();
  EXPECT_TRUE(seen_);
}

void RedirectNotificationObserver::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  source_ = source;
  details_ = details;
  seen_twice_ = seen_;
  seen_ = true;
  if (!running_)
    return;

  message_loop_runner_->Quit();
  running_ = false;
}

class SitePerProcessBrowserTest : public ContentBrowserTest {
 protected:
  bool NavigateIframeToURL(Shell* window,
                           const GURL& url,
                           std::string iframe_id) {
    std::string script = base::StringPrintf(
        "var iframes = document.getElementById('%s');iframes.src='%s';",
        iframe_id.c_str(), url.spec().c_str());
    WindowedNotificationObserver load_observer(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));
    bool result = ExecuteScript(window->web_contents(), script);
    load_observer.Wait();
    return result;
  }

  void NavigateToURLContentInitiated(Shell* window,
                                     const GURL& url,
                                     bool should_replace_current_entry) {
    std::string script;
    if (should_replace_current_entry)
      script = base::StringPrintf("location.replace('%s')", url.spec().c_str());
    else
      script = base::StringPrintf("location.href = '%s'", url.spec().c_str());
    TestNavigationObserver load_observer(shell()->web_contents(), 1);
    bool result = ExecuteScript(window->web_contents(), script);
    EXPECT_TRUE(result);
    load_observer.Wait();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kSitePerProcess);
  }
};

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, DISABLED_CrossSiteIframe) {
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));

  NavigateToURL(shell(), main_url);

  SitePerProcessWebContentsObserver observer(shell()->web_contents());
  {
    // Load same-site page into Iframe.
    GURL http_url(test_server()->GetURL("files/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell(), http_url, "test"));
    EXPECT_EQ(observer.navigation_url(), http_url);
    EXPECT_TRUE(observer.navigation_succeeded());
  }

  {
    // Load cross-site page into Iframe.
    GURL https_url(https_server.GetURL("files/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell(), https_url, "test"));
    EXPECT_EQ(observer.navigation_url(), https_url);
    EXPECT_FALSE(observer.navigation_succeeded());
  }
}

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DISABLED_CrossSiteIframeRedirectOnce) {
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));
  GURL http_url(test_server()->GetURL("files/title1.html"));
  GURL https_url(https_server.GetURL("files/title1.html"));

  NavigateToURL(shell(), main_url);

  SitePerProcessWebContentsObserver observer(shell()->web_contents());
  {
    // Load cross-site client-redirect page into Iframe.
    // Should be blocked.
    GURL client_redirect_https_url(https_server.GetURL(
        "client-redirect?files/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell(),
                                    client_redirect_https_url, "test"));
    // DidFailProvisionalLoad when navigating to client_redirect_https_url.
    EXPECT_EQ(observer.navigation_url(), client_redirect_https_url);
    EXPECT_FALSE(observer.navigation_succeeded());
  }

  {
    // Load cross-site server-redirect page into Iframe,
    // which redirects to same-site page.
    GURL server_redirect_http_url(https_server.GetURL(
        "server-redirect?" + http_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell(),
                                    server_redirect_http_url, "test"));
    EXPECT_EQ(observer.navigation_url(), http_url);
    EXPECT_TRUE(observer.navigation_succeeded());
  }

  {
    // Load cross-site server-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL server_redirect_http_url(https_server.GetURL(
        "server-redirect?files/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell(),
                                    server_redirect_http_url, "test"));
    // DidFailProvisionalLoad when navigating to https_url.
    EXPECT_EQ(observer.navigation_url(), https_url);
    EXPECT_FALSE(observer.navigation_succeeded());
  }

  {
    // Load same-site server-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?" + https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell(),
                                    server_redirect_http_url, "test"));

    EXPECT_EQ(observer.navigation_url(), https_url);
    EXPECT_FALSE(observer.navigation_succeeded());
   }

  {
    // Load same-site client-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL client_redirect_http_url(test_server()->GetURL(
        "client-redirect?" + https_url.spec()));

    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));

    EXPECT_TRUE(NavigateIframeToURL(shell(),
                                    client_redirect_http_url, "test"));

    // Same-site Client-Redirect Page should be loaded successfully.
    EXPECT_EQ(observer.navigation_url(), client_redirect_http_url);
    EXPECT_TRUE(observer.navigation_succeeded());

    // Redirecting to Cross-site Page should be blocked.
    load_observer2.Wait();
    EXPECT_EQ(observer.navigation_url(), https_url);
    EXPECT_FALSE(observer.navigation_succeeded());
  }

  {
    // Load same-site server-redirect page into Iframe,
    // which redirects to same-site page.
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?files/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell(),
                                    server_redirect_http_url, "test"));
    EXPECT_EQ(observer.navigation_url(), http_url);
    EXPECT_TRUE(observer.navigation_succeeded());
   }

  {
    // Load same-site client-redirect page into Iframe,
    // which redirects to same-site page.
    GURL client_redirect_http_url(test_server()->GetURL(
        "client-redirect?" + http_url.spec()));
    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));

    EXPECT_TRUE(NavigateIframeToURL(shell(),
                                    client_redirect_http_url, "test"));

    // Same-site Client-Redirect Page should be loaded successfully.
    EXPECT_EQ(observer.navigation_url(), client_redirect_http_url);
    EXPECT_TRUE(observer.navigation_succeeded());

    // Redirecting to Same-site Page should be loaded successfully.
    load_observer2.Wait();
    EXPECT_EQ(observer.navigation_url(), http_url);
    EXPECT_TRUE(observer.navigation_succeeded());
  }
}

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DISABLED_CrossSiteIframeRedirectTwice) {
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));
  GURL http_url(test_server()->GetURL("files/title1.html"));
  GURL https_url(https_server.GetURL("files/title1.html"));

  NavigateToURL(shell(), main_url);

  SitePerProcessWebContentsObserver observer(shell()->web_contents());
  {
    // Load client-redirect page pointing to a cross-site client-redirect page,
    // which eventually redirects back to same-site page.
    GURL client_redirect_https_url(https_server.GetURL(
        "client-redirect?" + http_url.spec()));
    GURL client_redirect_http_url(test_server()->GetURL(
        "client-redirect?" + client_redirect_https_url.spec()));

    // We should wait until second client redirect get cancelled.
    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));

    EXPECT_TRUE(NavigateIframeToURL(shell(), client_redirect_http_url, "test"));

    // DidFailProvisionalLoad when navigating to client_redirect_https_url.
    load_observer2.Wait();
    EXPECT_EQ(observer.navigation_url(), client_redirect_https_url);
    EXPECT_FALSE(observer.navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site server-redirect page,
    // which eventually redirect back to same-site page.
    GURL server_redirect_https_url(https_server.GetURL(
        "server-redirect?" + http_url.spec()));
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?" + server_redirect_https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell(),
                                    server_redirect_http_url, "test"));
    EXPECT_EQ(observer.navigation_url(), http_url);
    EXPECT_TRUE(observer.navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site server-redirect page,
    // which eventually redirects back to cross-site page.
    GURL server_redirect_https_url(https_server.GetURL(
        "server-redirect?" + https_url.spec()));
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?" + server_redirect_https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell(), server_redirect_http_url, "test"));

    // DidFailProvisionalLoad when navigating to https_url.
    EXPECT_EQ(observer.navigation_url(), https_url);
    EXPECT_FALSE(observer.navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site client-redirect page,
    // which eventually redirects back to same-site page.
    GURL client_redirect_http_url(https_server.GetURL(
        "client-redirect?" + http_url.spec()));
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?" + client_redirect_http_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell(), server_redirect_http_url, "test"));

    // DidFailProvisionalLoad when navigating to client_redirect_http_url.
    EXPECT_EQ(observer.navigation_url(), client_redirect_http_url);
    EXPECT_FALSE(observer.navigation_succeeded());
  }
}

// Ensures FrameTree correctly reflects page structure during navigations.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       FrameTreeShape) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = test_server()->GetURL("files/site_isolation/");
  GURL::Replacements replace_host;
  std::string host_str("A.com");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  // Load doc without iframes. Verify FrameTree just has root.
  // Frame tree:
  //   Site-A Root
  NavigateToURL(shell(), base_url.Resolve("blank.html"));
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
      GetFrameTree()->root();
  EXPECT_EQ(0U, root->child_count());

  // Add 2 same-site frames. Verify 3 nodes in tree with proper names.
  // Frame tree:
  //   Site-A Root -- Site-A frame1
  //              \-- Site-A frame2
  WindowedNotificationObserver observer1(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &shell()->web_contents()->GetController()));
  NavigateToURL(shell(), base_url.Resolve("frames-X-X.html"));
  observer1.Wait();
  ASSERT_EQ(2U, root->child_count());
  EXPECT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(0U, root->child_at(1)->child_count());
}

// TODO(ajwong): Talk with nasko and merge this functionality with
// FrameTreeShape.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       FrameTreeShape2) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  NavigateToURL(shell(),
                test_server()->GetURL("files/frame_tree/top.html"));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      wc->GetRenderViewHost());
  FrameTreeNode* root = wc->GetFrameTree()->root();

  // Check that the root node is properly created with the frame id of the
  // initial navigation.
  ASSERT_EQ(3UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());
  EXPECT_EQ(rvh->main_frame_id(), root->frame_id());

  ASSERT_EQ(2UL, root->child_at(0)->child_count());
  EXPECT_STREQ("1-1-name", root->child_at(0)->frame_name().c_str());

  // Verify the deepest node exists and has the right name.
  ASSERT_EQ(2UL, root->child_at(2)->child_count());
  EXPECT_EQ(1UL, root->child_at(2)->child_at(1)->child_count());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(1)->child_at(0)->child_count());
  EXPECT_STREQ("3-1-id",
      root->child_at(2)->child_at(1)->child_at(0)->frame_name().c_str());

  // Navigate to about:blank, which should leave only the root node of the frame
  // tree in the browser process.
  NavigateToURL(shell(), test_server()->GetURL("files/title1.html"));

  root = wc->GetFrameTree()->root();
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());
  EXPECT_EQ(rvh->main_frame_id(), root->frame_id());
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, FrameTreeAfterCrash) {
  ASSERT_TRUE(test_server()->Start());
  NavigateToURL(shell(),
                test_server()->GetURL("files/frame_tree/top.html"));

  // Crash the renderer so that it doesn't send any FrameDetached messages.
  WindowedNotificationObserver crash_observer(
      NOTIFICATION_RENDERER_PROCESS_CLOSED,
      NotificationService::AllSources());
  NavigateToURL(shell(), GURL(kChromeUICrashURL));
  crash_observer.Wait();

  // The frame tree should be cleared, and the frame ID should be reset.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      wc->GetRenderViewHost());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(FrameTreeNode::kInvalidFrameId, root->frame_id());
  EXPECT_EQ(rvh->main_frame_id(), root->frame_id());

  // Navigate to a new URL.
  NavigateToURL(shell(), test_server()->GetURL("files/title1.html"));

  // The frame ID should now be set.
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_NE(FrameTreeNode::kInvalidFrameId, root->frame_id());
  EXPECT_EQ(rvh->main_frame_id(), root->frame_id());
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, NavigateWithLeftoverFrames) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = test_server()->GetURL("files/site_isolation/");
  GURL::Replacements replace_host;
  std::string host_str("A.com");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  NavigateToURL(shell(),
                test_server()->GetURL("files/frame_tree/top.html"));

  // Hang the renderer so that it doesn't send any FrameDetached messages.
  // (This navigation will never complete, so don't wait for it.)
  shell()->LoadURL(GURL(kChromeUIHangURL));

  // Check that the frame tree still has children.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  ASSERT_EQ(3UL, root->child_count());

  // Navigate to a new URL.  We use LoadURL because NavigateToURL will try to
  // wait for the previous navigation to stop.
  TestNavigationObserver tab_observer(wc, 1);
  shell()->LoadURL(base_url.Resolve("blank.html"));
  tab_observer.Wait();

  // The frame tree should now be cleared, and the frame ID should be valid.
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_NE(FrameTreeNode::kInvalidFrameId, root->frame_id());
}

// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that began with a cross-process navigation.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ReplaceEntryCrossProcessThenTranfers) {
  const NavigationController& controller =
      shell()->web_contents()->GetController();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  // These must all stay in scope with replace_host.
  GURL::Replacements replace_host;
  std::string a_com("A.com");
  std::string b_com("B.com");

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url1 = test_server()->GetURL("files/site_isolation/blank.html?1");
  NavigateToURL(shell(), url1);

  // Force all future navigations to transfer. Note that this includes same-site
  // navigiations which may cause double process swaps (via OpenURL and then via
  // transfer). This test intentionally exercises that case.
  ShellContentBrowserClient::SetSwapProcessesForRedirect(true);

  // Navigate to a page on A.com with entry replacement. This navigation is
  // cross-site, so the renderer will send it to the browser via OpenURL to give
  // to a new process. It will then be transferred into yet another process due
  // to the call above.
  GURL url2 = test_server()->GetURL("files/site_isolation/blank.html?2");
  replace_host.SetHostStr(a_com);
  url2 = url2.ReplaceComponents(replace_host);
  NavigateToURLContentInitiated(shell(), url2, true);

  // There should be one history entry. url2 should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());

  // Now navigate as before to a page on B.com, but normally (without
  // replacement). This will still perform a double process-swap as above, via
  // OpenURL and then transfer.
  GURL url3 = test_server()->GetURL("files/site_isolation/blank.html?3");
  replace_host.SetHostStr(b_com);
  url3 = url3.ReplaceComponents(replace_host);
  NavigateToURLContentInitiated(shell(), url3, false);

  // There should be two history entries. url2 should have replaced url1. url2
  // should not have replaced url3.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());
}

// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that began with a content-initiated in-process
// navigation. This test is the same as the test above, except transfering from
// in-process.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ReplaceEntryInProcessThenTranfers) {
  const NavigationController& controller =
      shell()->web_contents()->GetController();
  ASSERT_TRUE(test_server()->Start());

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url = test_server()->GetURL("files/site_isolation/blank.html?1");
  NavigateToURL(shell(), url);

  // Force all future navigations to transfer. Note that this includes same-site
  // navigiations which may cause double process swaps (via OpenURL and then via
  // transfer). All navigations in this test are same-site, so it only swaps
  // processes via request transfer.
  ShellContentBrowserClient::SetSwapProcessesForRedirect(true);

  // Navigate in-process with entry replacement. It will then be transferred
  // into a new one due to the call above.
  GURL url2 = test_server()->GetURL("files/site_isolation/blank.html?2");
  NavigateToURLContentInitiated(shell(), url2, true);

  // There should be one history entry. url2 should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());

  // Now navigate as before, but without replacement.
  GURL url3 = test_server()->GetURL("files/site_isolation/blank.html?3");
  NavigateToURLContentInitiated(shell(), url3, false);

  // There should be two history entries. url2 should have replaced url1. url2
  // should not have replaced url3.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());
}

// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that cross processes twice from renderer policy.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ReplaceEntryCrossProcessTwice) {
  const NavigationController& controller =
      shell()->web_contents()->GetController();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  // These must all stay in scope with replace_host.
  GURL::Replacements replace_host;
  std::string a_com("A.com");
  std::string b_com("B.com");

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url1 = test_server()->GetURL("files/site_isolation/blank.html?1");
  NavigateToURL(shell(), url1);

  // Navigate to a page on A.com which redirects to B.com with entry
  // replacement. This will switch processes via OpenURL twice. First to A.com,
  // and second in response to the server redirect to B.com. The second swap is
  // also renderer-initiated via OpenURL because decidePolicyForNavigation is
  // currently applied on redirects.
  GURL url2b = test_server()->GetURL("files/site_isolation/blank.html?2");
  replace_host.SetHostStr(b_com);
  url2b = url2b.ReplaceComponents(replace_host);
  GURL url2a = test_server()->GetURL(
      "server-redirect?" + net::EscapeQueryParamValue(url2b.spec(), false));
  replace_host.SetHostStr(a_com);
  url2a = url2a.ReplaceComponents(replace_host);
  NavigateToURLContentInitiated(shell(), url2a, true);

  // There should be one history entry. url2b should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2b, controller.GetEntryAtIndex(0)->GetURL());

  // Now repeat without replacement.
  GURL url3b = test_server()->GetURL("files/site_isolation/blank.html?3");
  replace_host.SetHostStr(b_com);
  url3b = url3b.ReplaceComponents(replace_host);
  GURL url3a = test_server()->GetURL(
      "server-redirect?" + net::EscapeQueryParamValue(url3b.spec(), false));
  replace_host.SetHostStr(a_com);
  url3a = url3a.ReplaceComponents(replace_host);
  NavigateToURLContentInitiated(shell(), url3a, false);

  // There should be two history entries. url2b should have replaced url1. url2b
  // should not have replaced url3b.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2b, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3b, controller.GetEntryAtIndex(1)->GetURL());
}

}  // namespace content
