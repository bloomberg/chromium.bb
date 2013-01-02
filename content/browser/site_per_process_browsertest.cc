// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"

namespace content {

class SitePerProcessWebContentsObserver: public WebContentsObserver {
 public:
  explicit SitePerProcessWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        navigation_succeeded_(true) {}
  virtual ~SitePerProcessWebContentsObserver() {}

  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      RenderViewHost* render_view_host) OVERRIDE {
    navigation_url_ = validated_url;
    navigation_succeeded_ = false;
  }

  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
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
 public:
  SitePerProcessBrowserTest() {}

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
    bool result = ExecuteJavaScript(window->web_contents()->GetRenderViewHost(),
                                    L"", ASCIIToWide(script));
    load_observer.Wait();
    return result;
  }

  void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kSitePerProcess);
  }
};

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrossSiteIframe) {
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
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

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrossSiteIframeRedirectOnce) {
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
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

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CrossSiteIframeRedirectTwice) {
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
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

}
