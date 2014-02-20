// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_resource_dispatcher_host_delegate.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace content {

class SitePerProcessWebContentsObserver: public WebContentsObserver {
 public:
  explicit SitePerProcessWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        navigation_succeeded_(false) {}
  virtual ~SitePerProcessWebContentsObserver() {}

  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      RenderViewHost* render_view_host) OVERRIDE {
    navigation_succeeded_ = false;
  }

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

// Tracks a single request for a specified URL, and allows waiting until the
// request is destroyed, and then inspecting whether it completed successfully.
class TrackingResourceDispatcherHostDelegate
    : public ShellResourceDispatcherHostDelegate {
 public:
  TrackingResourceDispatcherHostDelegate() : throttle_created_(false) {
  }

  virtual void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      appcache::AppCacheService* appcache_service,
      ResourceType::Type resource_type,
      int child_id,
      int route_id,
      ScopedVector<ResourceThrottle>* throttles) OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ShellResourceDispatcherHostDelegate::RequestBeginning(
        request, resource_context, appcache_service, resource_type, child_id,
        route_id, throttles);
    // Expect only a single request for the tracked url.
    ASSERT_FALSE(throttle_created_);
    // If this is a request for the tracked URL, add a throttle to track it.
    if (request->url() == tracked_url_)
      throttles->push_back(new TrackingThrottle(request, this));
  }

  // Starts tracking a URL.  The request for previously tracked URL, if any,
  // must have been made and deleted before calling this function.
  void SetTrackedURL(const GURL& tracked_url) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Should not currently be tracking any URL.
    ASSERT_FALSE(run_loop_);

    // Create a RunLoop that will be stopped once the request for the tracked
    // URL has been destroyed, to allow tracking the URL while also waiting for
    // other events.
    run_loop_.reset(new base::RunLoop());

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &TrackingResourceDispatcherHostDelegate::SetTrackedURLOnIOThread,
            base::Unretained(this),
            tracked_url));
  }

  // Waits until the tracked URL has been requests, and the request for it has
  // been destroyed.
  bool WaitForTrackedURLAndGetCompleted() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    run_loop_->Run();
    run_loop_.reset();
    return tracked_request_completed_;
  }

 private:
  // ResourceThrottle attached to request for the tracked URL.  On destruction,
  // passes the final URLRequestStatus back to the delegate.
  class TrackingThrottle : public ResourceThrottle {
   public:
    TrackingThrottle(net::URLRequest* request,
                     TrackingResourceDispatcherHostDelegate* tracker)
        : request_(request), tracker_(tracker) {
    }

    virtual ~TrackingThrottle() {
      // If the request is deleted without being cancelled, its status will
      // indicate it succeeded, so have to check if the request is still pending
      // as well.
      tracker_->OnTrackedRequestDestroyed(
          !request_->is_pending() && request_->status().is_success());
    }

    // ResourceThrottle implementation:
    virtual const char* GetNameForLogging() const OVERRIDE {
      return "TrackingThrottle";
    }

   private:
    net::URLRequest* request_;
    TrackingResourceDispatcherHostDelegate* tracker_;

    DISALLOW_COPY_AND_ASSIGN(TrackingThrottle);
  };

  void SetTrackedURLOnIOThread(const GURL& tracked_url) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    throttle_created_ = false;
    tracked_url_ = tracked_url;
  }

  void OnTrackedRequestDestroyed(bool completed) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    tracked_request_completed_ = completed;
    tracked_url_ = GURL();

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, run_loop_->QuitClosure());
  }

  // These live on the IO thread.
  GURL tracked_url_;
  bool throttle_created_;

  // This is created and destroyed on the UI thread, but stopped on the IO
  // thread.
  scoped_ptr<base::RunLoop> run_loop_;

  // Set on the IO thread while |run_loop_| is non-NULL, read on the UI thread
  // after deleting run_loop_.
  bool tracked_request_completed_;

  DISALLOW_COPY_AND_ASSIGN(TrackingResourceDispatcherHostDelegate);
};

// WebContentsDelegate that fails to open a URL when there's a request that
// needs to be transferred between renderers.
class NoTransferRequestDelegate : public WebContentsDelegate {
 public:
  NoTransferRequestDelegate() {}

  virtual WebContents* OpenURLFromTab(WebContents* source,
                                      const OpenURLParams& params) OVERRIDE {
    bool is_transfer =
        (params.transferred_global_request_id != GlobalRequestID());
    if (is_transfer)
      return NULL;
    NavigationController::LoadURLParams load_url_params(params.url);
    load_url_params.referrer = params.referrer;
    load_url_params.frame_tree_node_id = params.frame_tree_node_id;
    load_url_params.transition_type = params.transition;
    load_url_params.extra_headers = params.extra_headers;
    load_url_params.should_replace_current_entry =
        params.should_replace_current_entry;
    load_url_params.is_renderer_initiated = true;
    source->GetController().LoadURLWithParams(load_url_params);
    return source;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoTransferRequestDelegate);
};

class SitePerProcessBrowserTest : public ContentBrowserTest {
 public:
  SitePerProcessBrowserTest() : old_delegate_(NULL) {
  }

  // ContentBrowserTest implementation:
  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &SitePerProcessBrowserTest::InjectResourceDisptcherHostDelegate,
            base::Unretained(this)));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &SitePerProcessBrowserTest::RestoreResourceDisptcherHostDelegate,
            base::Unretained(this)));
  }

 protected:
  // Start at a data URL so each extra navigation creates a navigation entry.
  // (The first navigation will silently be classified as AUTO_SUBFRAME.)
  // TODO(creis): This won't be necessary when we can wait for LOAD_STOP.
  void StartFrameAtDataURL() {
    std::string data_url_script =
      "var iframes = document.getElementById('test');iframes.src="
      "'data:text/html,dataurl';";
    ASSERT_TRUE(ExecuteScript(shell()->web_contents(), data_url_script));
  }

  bool NavigateIframeToURL(Shell* window,
                           const GURL& url,
                           std::string iframe_id) {
    // TODO(creis): This should wait for LOAD_STOP, but cross-site subframe
    // navigations generate extra DidStartLoading and DidStopLoading messages.
    // Until we replace swappedout:// with frame proxies, we need to listen for
    // something else.  For now, we trigger NEW_SUBFRAME navigations and listen
    // for commit.
    std::string script = base::StringPrintf(
        "setTimeout(\""
        "var iframes = document.getElementById('%s');iframes.src='%s';"
        "\",0)",
        iframe_id.c_str(), url.spec().c_str());
    WindowedNotificationObserver load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(
            &window->web_contents()->GetController()));
    bool result = ExecuteScript(window->web_contents(), script);
    load_observer.Wait();
    return result;
  }

  void NavigateToURLContentInitiated(Shell* window,
                                     const GURL& url,
                                     bool should_replace_current_entry,
                                     bool should_wait_for_navigation) {
    std::string script;
    if (should_replace_current_entry)
      script = base::StringPrintf("location.replace('%s')", url.spec().c_str());
    else
      script = base::StringPrintf("location.href = '%s'", url.spec().c_str());
    TestNavigationObserver load_observer(shell()->web_contents(), 1);
    bool result = ExecuteScript(window->web_contents(), script);
    EXPECT_TRUE(result);
    if (should_wait_for_navigation)
      load_observer.Wait();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kSitePerProcess);

    // TODO(creis): Remove this when GTK is no longer a supported platform.
    command_line->AppendSwitch(switches::kForceCompositingMode);
  }

  void InjectResourceDisptcherHostDelegate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    old_delegate_ = ResourceDispatcherHostImpl::Get()->delegate();
    ResourceDispatcherHostImpl::Get()->SetDelegate(&tracking_delegate_);
  }

  void RestoreResourceDisptcherHostDelegate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ResourceDispatcherHostImpl::Get()->SetDelegate(old_delegate_);
    old_delegate_ = NULL;
  }

  TrackingResourceDispatcherHostDelegate& tracking_delegate() {
    return tracking_delegate_;
  }

 private:
  TrackingResourceDispatcherHostDelegate tracking_delegate_;
  ResourceDispatcherHostDelegate* old_delegate_;
};

// Ensure that we can complete a cross-process subframe navigation.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrossSiteIframe) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  StartFrameAtDataURL();

  SitePerProcessWebContentsObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  GURL http_url(test_server()->GetURL("files/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(shell(), http_url, "test"));
  EXPECT_EQ(http_url, observer.navigation_url());
  EXPECT_TRUE(observer.navigation_succeeded());

  // These must stay in scope with replace_host.
  GURL::Replacements replace_host;
  std::string foo_com("foo.com");

  // Load cross-site page into iframe.
  GURL cross_site_url(test_server()->GetURL("files/title2.html"));
  replace_host.SetHostStr(foo_com);
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  EXPECT_TRUE(NavigateIframeToURL(shell(), cross_site_url, "test"));
  EXPECT_EQ(cross_site_url, observer.navigation_url());
  EXPECT_TRUE(observer.navigation_succeeded());

  // Ensure that we have created a new process for the subframe.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(),
            child->current_frame_host()->render_view_host());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->render_view_host()->GetSiteInstance());
  EXPECT_NE(shell()->web_contents()->GetRenderProcessHost(),
            child->current_frame_host()->GetProcess());
}

// Crash a subframe and ensures its children are cleared from the FrameTree.
// See http://crbug.com/338508.
// TODO(creis): Enable this on Android when we can kill the process there.
#if defined(OS_ANDROID)
#define MAYBE_CrashSubframe DISABLED_CrashSubframe
#else
#define MAYBE_CrashSubframe CrashSubframe
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, MAYBE_CrashSubframe) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  StartFrameAtDataURL();

  // These must stay in scope with replace_host.
  GURL::Replacements replace_host;
  std::string foo_com("foo.com");

  // Load cross-site page into iframe.
  GURL cross_site_url(test_server()->GetURL("files/title2.html"));
  replace_host.SetHostStr(foo_com);
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  EXPECT_TRUE(NavigateIframeToURL(shell(), cross_site_url, "test"));

  // Check the subframe process.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(cross_site_url, child->current_url());

  // Crash the subframe process.
  RenderProcessHost* root_process = root->current_frame_host()->GetProcess();
  RenderProcessHost* child_process = child->current_frame_host()->GetProcess();
  {
    RenderProcessHostWatcher crash_observer(
        child_process,
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    base::KillProcess(child_process->GetHandle(), 0, false);
    crash_observer.Wait();
  }

  // Ensure that the child frame still exists but has been cleared.
  EXPECT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(GURL(), child->current_url());

  // Now crash the top-level page to clear the child frame.
  {
    RenderProcessHostWatcher crash_observer(
        root_process,
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    base::KillProcess(root_process->GetHandle(), 0, false);
    crash_observer.Wait();
  }
  EXPECT_EQ(0U, root->child_count());
  EXPECT_EQ(GURL(), root->current_url());
}

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
// TODO(creis): Replace SpawnedTestServer with host_resolver to get test to run
// on Android (http://crbug.com/187570).
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
// TODO(creis): Replace SpawnedTestServer with host_resolver to get test to run
// on Android (http://crbug.com/187570).
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

// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that began with a cross-process navigation.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ReplaceEntryCrossProcessThenTransfer) {
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
  // Used to make sure the request for url2 succeeds, and there was only one of
  // them.
  tracking_delegate().SetTrackedURL(url2);
  NavigateToURLContentInitiated(shell(), url2, true, true);

  // There should be one history entry. url2 should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  // Make sure the request succeeded.
  EXPECT_TRUE(tracking_delegate().WaitForTrackedURLAndGetCompleted());

  // Now navigate as before to a page on B.com, but normally (without
  // replacement). This will still perform a double process-swap as above, via
  // OpenURL and then transfer.
  GURL url3 = test_server()->GetURL("files/site_isolation/blank.html?3");
  replace_host.SetHostStr(b_com);
  url3 = url3.ReplaceComponents(replace_host);
  // Used to make sure the request for url3 succeeds, and there was only one of
  // them.
  tracking_delegate().SetTrackedURL(url3);
  NavigateToURLContentInitiated(shell(), url3, false, true);

  // There should be two history entries. url2 should have replaced url1. url2
  // should not have replaced url3.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());

  // Make sure the request succeeded.
  EXPECT_TRUE(tracking_delegate().WaitForTrackedURLAndGetCompleted());
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
  NavigateToURLContentInitiated(shell(), url2, true, true);

  // There should be one history entry. url2 should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());

  // Now navigate as before, but without replacement.
  GURL url3 = test_server()->GetURL("files/site_isolation/blank.html?3");
  NavigateToURLContentInitiated(shell(), url3, false, true);

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
  NavigateToURLContentInitiated(shell(), url2a, true, true);

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
  NavigateToURLContentInitiated(shell(), url3a, false, true);

  // There should be two history entries. url2b should have replaced url1. url2b
  // should not have replaced url3b.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2b, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3b, controller.GetEntryAtIndex(1)->GetURL());
}

// Tests that the request is destroyed when a cross process navigation is
// cancelled.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, NoLeakOnCrossSiteCancel) {
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

  // Force all future navigations to transfer.
  ShellContentBrowserClient::SetSwapProcessesForRedirect(true);

  NoTransferRequestDelegate no_transfer_request_delegate;
  WebContentsDelegate* old_delegate = shell()->web_contents()->GetDelegate();
  shell()->web_contents()->SetDelegate(&no_transfer_request_delegate);

  // Navigate to a page on A.com with entry replacement. This navigation is
  // cross-site, so the renderer will send it to the browser via OpenURL to give
  // to a new process. It will then be transferred into yet another process due
  // to the call above.
  GURL url2 = test_server()->GetURL("files/site_isolation/blank.html?2");
  replace_host.SetHostStr(a_com);
  url2 = url2.ReplaceComponents(replace_host);
  // Used to make sure the second request is cancelled, and there is only one
  // request for url2.
  tracking_delegate().SetTrackedURL(url2);

  // Don't wait for the navigation to complete, since that never happens in
  // this case.
  NavigateToURLContentInitiated(shell(), url2, false, false);

  // There should be one history entry, with url1.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());

  // Make sure the request for url2 did not complete.
  EXPECT_FALSE(tracking_delegate().WaitForTrackedURLAndGetCompleted());

  shell()->web_contents()->SetDelegate(old_delegate);
}

}  // namespace content
