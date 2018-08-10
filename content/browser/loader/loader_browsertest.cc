// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_dispatcher_host.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using testing::HasSubstr;
using testing::Not;

namespace content {

class LoaderBrowserTest : public ContentBrowserTest,
                          public DownloadManager::Observer {
 public:
  LoaderBrowserTest() : got_downloads_(false) {}

 protected:
  void SetUpOnMainThread() override {
    base::FilePath path = GetTestFilePath("", "");
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&net::URLRequestMockHTTPJob::AddUrlHandlers, path));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&net::URLRequestFailedJob::AddUrlHandler));
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void OnDownloadCreated(DownloadManager* manager,
                         download::DownloadItem* item) override {
    if (!got_downloads_)
      got_downloads_ = !!manager->InProgressCount();
  }

  void CheckTitleTest(const GURL& url, const std::string& expected_title) {
    base::string16 expected_title16(ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    NavigateToURL(shell(), url);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  bool GetPopupTitle(const GURL& url, base::string16* title) {
    NavigateToURL(shell(), url);

    ShellAddedObserver new_shell_observer;

    // Create dynamic popup.
    if (!ExecuteScript(shell(), "OpenPopup();"))
      return false;

    Shell* new_shell = new_shell_observer.GetShell();
    *title = new_shell->web_contents()->GetTitle();
    return true;
  }

  std::string GetCookies(const GURL& url) {
    return content::GetCookies(shell()->web_contents()->GetBrowserContext(),
                               url);
  }

  bool got_downloads() const { return got_downloads_; }

 private:
  bool got_downloads_;
};

// Test title for content created by javascript window.open().
// See http://crbug.com/5988
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DynamicTitle1) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/dynamic1.html"));
  base::string16 title;
  ASSERT_TRUE(GetPopupTitle(url, &title));
  EXPECT_TRUE(base::StartsWith(title, ASCIIToUTF16("My Popup Title"),
                               base::CompareCase::SENSITIVE))
      << "Actual title: " << title;
}

// Test title for content created by javascript window.open().
// See http://crbug.com/5988
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DynamicTitle2) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/dynamic2.html"));
  base::string16 title;
  ASSERT_TRUE(GetPopupTitle(url, &title));
  EXPECT_TRUE(base::StartsWith(title, ASCIIToUTF16("My Dynamic Title"),
                               base::CompareCase::SENSITIVE))
      << "Actual title: " << title;
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SniffHTMLWithNoContentType) {
  // Covered by URLLoaderTest.SniffMimeType.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test0.html"),
      "Content Sniffer Test 0");
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, RespectNoSniffDirective) {
  // Covered by URLLoaderTest.RespectNoSniff.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  CheckTitleTest(net::URLRequestMockHTTPJob::GetMockUrl("nosniff-test.html"),
                 "mock.http/nosniff-test.html");
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DoNotSniffHTMLFromTextPlain) {
  // Covered by URLLoaderTest.DoNotSniffHTMLFromTextPlain.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test1.html"),
      "mock.http/content-sniffer-test1.html");
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DoNotSniffHTMLFromImageGIF) {
  // Covered by URLLoaderTest.DoNotSniffHTMLFromImageGIF.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test2.html"),
      "mock.http/content-sniffer-test2.html");
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SniffNoContentTypeNoData) {
  // Make sure no downloads start.
  BrowserContext::GetDownloadManager(
      shell()->web_contents()->GetBrowserContext())
      ->AddObserver(this);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/content-sniffer-test3.html"));
  CheckTitleTest(url, "Content Sniffer Test 3");
  EXPECT_EQ(1u, Shell::windows().size());
  ASSERT_FALSE(got_downloads());
}

// Make sure file URLs are not sniffed as HTML when they don't end in HTML.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DoNotSniffHTMLFromFileUrl) {
  base::FilePath path =
      GetTestFilePath(nullptr, "content-sniffer-test5.not-html");
  GURL file_url = net::FilePathToFileURL(path);
  // If the file isn't rendered as HTML, the title will match the name of the
  // file, rather than the contents of the file's title tag.
  CheckTitleTest(file_url, path.BaseName().MaybeAsASCII());
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, ContentDispositionEmpty) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/content-disposition-empty.html"));
  CheckTitleTest(url, "success");
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, ContentDispositionInline) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/content-disposition-inline.html"));
  CheckTitleTest(url, "success");
}

// Test for bug #1091358.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SyncXMLHttpRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/sync_xmlhttprequest.html"));

  // Let's check the XMLHttpRequest ran successfully.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(DidSyncRequestSucceed());",
      &success));
  EXPECT_TRUE(success);
}

// If this flakes, use http://crbug.com/62776.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SyncXMLHttpRequest_Disallowed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL(
                             "/sync_xmlhttprequest_disallowed.html"));

  // Let's check the XMLHttpRequest ran successfully.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(DidSucceed());", &success));
  EXPECT_TRUE(success);
}

// Test for bug #1159553 -- A synchronous xhr (whose content-type is
// downloadable) would trigger download and hang the renderer process,
// if executed while navigating to a new page.
// Disabled on Mac: see http://crbug.com/56264
#if defined(OS_MACOSX)
#define MAYBE_SyncXMLHttpRequest_DuringUnload \
  DISABLED_SyncXMLHttpRequest_DuringUnload
#else
#define MAYBE_SyncXMLHttpRequest_DuringUnload SyncXMLHttpRequest_DuringUnload
#endif
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       MAYBE_SyncXMLHttpRequest_DuringUnload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  BrowserContext::GetDownloadManager(
      shell()->web_contents()->GetBrowserContext())
      ->AddObserver(this);

  CheckTitleTest(
      embedded_test_server()->GetURL("/sync_xmlhttprequest_during_unload.html"),
      "sync xhr on unload");

  // Navigate to a new page, to dispatch unload event and trigger xhr.
  // (the bug would make this step hang the renderer).
  CheckTitleTest(embedded_test_server()->GetURL("/title2.html"),
                 "Title Of Awesomeness");

  ASSERT_FALSE(got_downloads());
}

namespace {

// Responds with a HungResponse for the specified URL to hang on the request.
// If the network service is enabled, crashes the process. If it's disabled,
// cancels all requests from specifield |child_id|.
//
// |crash_network_service_callback| crashes the network service when invoked,
// and must be called on the UI thread.
std::unique_ptr<net::test_server::HttpResponse> CancelOnRequest(
    const std::string& relative_url,
    int child_id,
    base::RepeatingClosure crash_network_service_callback,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != relative_url)
    return nullptr;

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     crash_network_service_callback);
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ResourceDispatcherHostImpl::CancelRequestsForProcess,
                       base::Unretained(ResourceDispatcherHostImpl::Get()),
                       child_id));
  }

  return std::make_unique<net::test_server::HungResponse>();
}

}  // namespace

// Tests the case where the request is cancelled by a layer above the
// URLRequest, which passes the error on ResourceLoader teardown, rather than in
// response to call to AsyncResourceHandler::OnResponseComplete.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SyncXMLHttpRequest_Cancelled) {
  embedded_test_server()->RegisterRequestHandler(base::Bind(
      &CancelOnRequest, "/hung",
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(),
      base::BindRepeating(&BrowserTestBase::SimulateNetworkServiceCrash,
                          base::Unretained(this))));

  ASSERT_TRUE(embedded_test_server()->Start());
  WaitForLoadStop(shell()->web_contents());

  NavigateToURL(shell(), embedded_test_server()->GetURL(
                             "/sync_xmlhttprequest_cancelled.html"));

  int status_code = -1;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      shell(), "window.domAutomationController.send(getErrorCode());",
      &status_code));

  // 19 is the value of NETWORK_ERROR on DOMException.
  EXPECT_EQ(19, status_code);
}

// Flaky everywhere. http://crbug.com/130404
// Tests that onunload is run for cross-site requests.  (Bug 1114994)
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, DISABLED_CrossSiteOnunloadCookie) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/onunload_cookie.html");
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a new cross-site page, to dispatch unload event and set the
  // cookie.
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test0.html"),
      "Content Sniffer Test 0");

  // Check that the cookie was set.
  EXPECT_EQ("onunloadCookie=foo", GetCookies(url));
}

// If this flakes, use http://crbug.com/130404
// Tests that onunload is run for cross-site requests to URLs that complete
// without network loads (e.g., about:blank, data URLs).
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       DISABLED_CrossSiteImmediateLoadOnunloadCookie) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/onunload_cookie.html");
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a cross-site page that loads immediately without making a
  // network request.  The unload event should still be run.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Check that the cookie was set.
  EXPECT_EQ("onunloadCookie=foo", GetCookies(url));
}

namespace {

// Handles |request| by serving a redirect response.
std::unique_ptr<net::test_server::HttpResponse> NoContentResponseHandler(
    const std::string& path,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(path, request.relative_url,
                        base::CompareCase::SENSITIVE))
    return std::unique_ptr<net::test_server::HttpResponse>();

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_NO_CONTENT);
  return std::move(http_response);
}

}  // namespace

// Tests that the unload handler is not run for 204 responses.
// If this flakes use http://crbug.com/80596.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossSiteNoUnloadOn204) {
  const char kNoContentPath[] = "/nocontent";
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&NoContentResponseHandler, kNoContentPath));

  ASSERT_TRUE(embedded_test_server()->Start());

  // Start with a URL that sets a cookie in its unload handler.
  GURL url = embedded_test_server()->GetURL("/onunload_cookie.html");
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a cross-site URL that returns a 204 No Content response.
  NavigateToURL(shell(), embedded_test_server()->GetURL(kNoContentPath));

  // Check that the unload cookie was not set.
  EXPECT_EQ("", GetCookies(url));
}

// Tests that the onbeforeunload and onunload logic is short-circuited if the
// old renderer is gone.  In that case, we don't want to wait for the old
// renderer to run the handlers.
// We need to disable this on Mac because the crash causes the OS CrashReporter
// process to kick in to analyze the poor dead renderer.  Unfortunately, if the
// app isn't stripped of debug symbols, this takes about five minutes to
// complete and isn't conducive to quick turnarounds. As we don't currently
// strip the app on the build bots, this is bad times.
#if defined(OS_MACOSX)
#define MAYBE_CrossSiteAfterCrash DISABLED_CrossSiteAfterCrash
#else
#define MAYBE_CrossSiteAfterCrash CrossSiteAfterCrash
#endif
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, MAYBE_CrossSiteAfterCrash) {
  // Make sure we have a live process before trying to kill it.
  NavigateToURL(shell(), GURL("about:blank"));

  // Cause the renderer to crash.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  NavigateToURL(shell(), GURL(kChromeUICrashURL));
  // Wait for browser to notice the renderer crash.
  crash_observer.Wait();

  // Navigate to a new cross-site page.  The browser should not wait around for
  // the old renderer's on{before}unload handlers to run.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/content-sniffer-test0.html"));
  CheckTitleTest(url, "Content Sniffer Test 0");
}

// Tests that cross-site navigations work when the new page does not go through
// the BufferedEventHandler (e.g., non-http{s} URLs).  (Bug 1225872)
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossSiteNavigationNonBuffered) {
  // Start with an HTTP page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("/content-sniffer-test0.html"));
  CheckTitleTest(url1, "Content Sniffer Test 0");

  // Now load a file:// page, which does not use the BufferedEventHandler.
  // Make sure that the page loads and displays a title, and doesn't get stuck.
  GURL url2 = GetTestUrl("", "title2.html");
  CheckTitleTest(url2, "Title Of Awesomeness");
}

// Flaky everywhere. http://crbug.com/130404
// Tests that a cross-site navigation to an error page (resulting in the link
// doctor page) still runs the onunload handler and can support navigations
// away from the link doctor page.  (Bug 1235537)
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       DISABLED_CrossSiteNavigationErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/onunload_cookie.html"));
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a new cross-site URL that results in an error.
  // TODO(creis): If this causes crashes or hangs, it might be for the same
  // reason as ErrorPageTest::DNSError.  See bug 1199491 and
  // http://crbug.com/22877.
  GURL failed_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  NavigateToURL(shell(), failed_url);

  EXPECT_NE(ASCIIToUTF16("set cookie on unload"),
            shell()->web_contents()->GetTitle());

  // Check that the cookie was set, meaning that the onunload handler ran.
  EXPECT_EQ("onunloadCookie=foo", GetCookies(url));

  // Check that renderer-initiated navigations still work.  In a previous bug,
  // the ResourceDispatcherHost would think that such navigations were
  // cross-site, because we didn't clean up from the previous request.  Since
  // WebContentsImpl was in the NORMAL state, it would ignore the attempt to run
  // the onunload handler, and the navigation would fail. We can't test by
  // redirecting to javascript:window.location='someURL', since javascript:
  // URLs are prohibited by policy from interacting with sensitive chrome
  // pages of which the error page is one.  Instead, use automation to kick
  // off the navigation, and wait to see that the tab loads.
  base::string16 expected_title16(ASCIIToUTF16("Title Of Awesomeness"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);

  bool success;
  GURL test_url(embedded_test_server()->GetURL("/title2.html"));
  std::string redirect_script = "window.location='" +
                                test_url.possibly_invalid_spec() + "';" +
                                "window.domAutomationController.send(true);";
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(), redirect_script, &success));
  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossSiteNavigationErrorPage2) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title2.html"));
  CheckTitleTest(url, "Title Of Awesomeness");

  // Navigate to a new cross-site URL that results in an error.
  // TODO(creis): If this causes crashes or hangs, it might be for the same
  // reason as ErrorPageTest::DNSError.  See bug 1199491 and
  // http://crbug.com/22877.
  GURL failed_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);

  NavigateToURL(shell(), failed_url);
  EXPECT_NE(ASCIIToUTF16("Title Of Awesomeness"),
            shell()->web_contents()->GetTitle());

  // Repeat navigation.  We are testing that this completes.
  NavigateToURL(shell(), failed_url);
  EXPECT_NE(ASCIIToUTF16("Title Of Awesomeness"),
            shell()->web_contents()->GetTitle());
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossOriginRedirectBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/cross-origin-redirect-blocked.html"));
  // We expect the following URL requests from this test:
  // 1- navigation to http://127.0.0.1:[port]/cross-origin-redirect-blocked.html
  // 2- XHR to
  // http://127.0.0.1:[port]/server-redirect-302?http://a.com:[port]/title2.html
  // 3- above XHR is redirected to http://a.com:[port]/title2.html which should
  // be blocked
  // 4- When the page notices the above request is blocked, it issues an XHR to
  // http://127.0.0.1:[port]/title2.html
  // 5- When the above XHR succeed, the page navigates to
  // http://127.0.0.1:[port]/title3.html
  //
  // If the redirect in #3 were not blocked, we'd instead see a navigation
  // to http://a.com[port]/title2.html, and the title would be different.
  CheckTitleTest(url, "Title Of More Awesomeness");
}

// Tests that ResourceRequestInfoImpl is updated correctly on failed
// requests, to prevent calling Read on a request that has already failed.
// See bug 40250.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CrossSiteFailedRequest) {
  // Visit another URL first to trigger a cross-site navigation.
  NavigateToURL(shell(), GetTestUrl("", "simple_page.html"));

  // Visit a URL that fails without calling ResourceDispatcherHost::Read.
  GURL broken_url("chrome://theme");
  NavigateToURL(shell(), broken_url);
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
    const std::string& request_path,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, request_path,
                        base::CompareCase::SENSITIVE))
    return std::unique_ptr<net::test_server::HttpResponse>();

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_FOUND);
  http_response->AddCustomHeader(
      "Location", request.relative_url.substr(request_path.length()));
  return std::move(http_response);
}

}  // namespace

// Test that we update the cookie policy URLs correctly when transferring
// navigations.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, CookiePolicy) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&HandleRedirectRequest, "/redirect?"));
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string set_cookie_url(base::StringPrintf(
      "http://localhost:%u/set_cookie.html", embedded_test_server()->port()));
  GURL url(embedded_test_server()->GetURL("/redirect?" + set_cookie_url));

  ShellNetworkDelegate::SetBlockThirdPartyCookies(true);

  CheckTitleTest(url, "cookie set");
}

class PageTransitionResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  explicit PageTransitionResourceDispatcherHostDelegate(GURL watch_url)
      : watch_url_(watch_url) {}

  // ResourceDispatcherHostDelegate implementation:
  void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      AppCacheService* appcache_service,
      ResourceType resource_type,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles) override {
    if (request->url() == watch_url_) {
      const ResourceRequestInfo* info =
          ResourceRequestInfo::ForRequest(request);
      page_transition_ = info->GetPageTransition();
    }
  }

  ui::PageTransition page_transition() { return page_transition_; }

 private:
  GURL watch_url_;
  ui::PageTransition page_transition_;
};

// Test that ui::PAGE_TRANSITION_CLIENT_REDIRECT is correctly set
// when encountering a meta refresh tag.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, PageTransitionClientRedirect) {
  // TODO(crbug.com/818445): Fix the flakiness on Network Service.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  PageTransitionResourceDispatcherHostDelegate delegate(
      embedded_test_server()->GetURL("/title1.html"));
  ResourceDispatcherHost::Get()->SetDelegate(&delegate);

  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/client_redirect.html"), 2);

  EXPECT_TRUE(delegate.page_transition() & ui::PAGE_TRANSITION_CLIENT_REDIRECT);
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, SubresourceRedirectToDataURLBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  GURL subresource_url = embedded_test_server()->GetURL(
      "/server-redirect?data:text/plain,redirected1");
  std::string script = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = () => domAutomationController.send("ALLOWED");
    xhr.onerror = () => domAutomationController.send("BLOCKED");
    xhr.send();
  }))";
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell(), script + "('" + subresource_url.spec() + "')", &result));

  EXPECT_EQ("BLOCKED", result);
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, RedirectToDataURLBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_FALSE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/server-redirect?data:text/plain,redirected1")));
}

namespace {

// Creates a valid filesystem URL.
GURL CreateFileSystemURL(Shell* window) {
  std::string filesystem_url_string;
  EXPECT_TRUE(
      ExecuteScriptAndExtractString(window, R"(
      var blob = new Blob(['<html><body>hello</body></html>'],
                          {type: 'text/html'});
      window.webkitRequestFileSystem(TEMPORARY, blob.size, fs => {
        fs.root.getFile('foo.html', {create: true}, file => {
          file.createWriter(writer => {
            writer.write(blob);
            writer.onwriteend = () => {
              domAutomationController.send(file.toURL());
            }
          });
        });
      });)", &filesystem_url_string));
  GURL filesystem_url(filesystem_url_string);
  EXPECT_TRUE(filesystem_url.is_valid());
  EXPECT_TRUE(filesystem_url.SchemeIsFileSystem());
  return filesystem_url;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       SubresourceRedirectToFileSystemURLBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  GURL subresource_url = embedded_test_server()->GetURL(
      "/server-redirect?" + CreateFileSystemURL(shell()).spec());
  std::string script = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = () => domAutomationController.send("ALLOWED");
    xhr.onerror = () => domAutomationController.send("BLOCKED");
    xhr.send();
  }))";
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell(), script + "('" + subresource_url.spec() + "')", &result));

  EXPECT_EQ("BLOCKED", result);
}

IN_PROC_BROWSER_TEST_F(LoaderBrowserTest, RedirectToFileSystemURLBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Need to navigate to a URL first so the filesystem can be created.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));

  EXPECT_FALSE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/server-redirect?" + CreateFileSystemURL(shell()).spec())));
}

namespace {

// Checks whether the given urls are requested, and that GetPreviewsState()
// returns the appropriate value when the Previews are set.
class PreviewsStateResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  PreviewsStateResourceDispatcherHostDelegate(const GURL& main_frame_url,
                                              const GURL& subresource_url,
                                              const GURL& iframe_url)
      : main_frame_url_(main_frame_url),
        subresource_url_(subresource_url),
        iframe_url_(iframe_url),
        main_frame_url_seen_(false),
        subresource_url_seen_(false),
        iframe_url_seen_(false),
        previews_state_(PREVIEWS_OFF),
        should_get_previews_state_called_(false) {}

  ~PreviewsStateResourceDispatcherHostDelegate() override {}

  // ResourceDispatcherHostDelegate implementation:
  void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      AppCacheService* appcache_service,
      ResourceType resource_type,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
    if (request->url() != main_frame_url_ &&
        request->url() != subresource_url_ && request->url() != iframe_url_)
      return;
    if (request->url() == main_frame_url_) {
      EXPECT_FALSE(main_frame_url_seen_);
      main_frame_url_seen_ = true;
    } else if (request->url() == subresource_url_) {
      EXPECT_TRUE(main_frame_url_seen_);
      EXPECT_FALSE(subresource_url_seen_);
      subresource_url_seen_ = true;
    } else if (request->url() == iframe_url_) {
      EXPECT_TRUE(main_frame_url_seen_);
      EXPECT_FALSE(iframe_url_seen_);
      iframe_url_seen_ = true;
    }
    EXPECT_EQ(previews_state_, info->GetPreviewsState());
  }

  void SetDelegate() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ResourceDispatcherHost::Get()->SetDelegate(this);
  }

  PreviewsState DetermineEnabledPreviews(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::PreviewsState previews_to_allow) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    EXPECT_FALSE(should_get_previews_state_called_);
    should_get_previews_state_called_ = true;
    EXPECT_EQ(main_frame_url_, request->url());
    return previews_state_;
  }

  void Reset(PreviewsState previews_state) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    main_frame_url_seen_ = false;
    subresource_url_seen_ = false;
    iframe_url_seen_ = false;
    previews_state_ = previews_state;
    should_get_previews_state_called_ = false;
  }

  void CheckResourcesRequested(bool should_get_previews_state_called) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    EXPECT_EQ(should_get_previews_state_called,
              should_get_previews_state_called_);
    EXPECT_TRUE(main_frame_url_seen_);
    EXPECT_TRUE(subresource_url_seen_);
    EXPECT_TRUE(iframe_url_seen_);
  }

 private:
  const GURL main_frame_url_;
  const GURL subresource_url_;
  const GURL iframe_url_;

  bool main_frame_url_seen_;
  bool subresource_url_seen_;
  bool iframe_url_seen_;
  PreviewsState previews_state_;
  bool should_get_previews_state_called_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsStateResourceDispatcherHostDelegate);
};

}  // namespace

class PreviewsStateBrowserTest : public ContentBrowserTest {
 public:
  ~PreviewsStateBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    delegate_.reset(new PreviewsStateResourceDispatcherHostDelegate(
        embedded_test_server()->GetURL("/page_with_iframe.html"),
        embedded_test_server()->GetURL("/image.jpg"),
        embedded_test_server()->GetURL("/title1.html")));

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &PreviewsStateResourceDispatcherHostDelegate::SetDelegate,
            base::Unretained(delegate_.get())));
  }

  void Reset(PreviewsState previews_state) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&PreviewsStateResourceDispatcherHostDelegate::Reset,
                       base::Unretained(delegate_.get()), previews_state));
  }

  void CheckResourcesRequested(bool should_get_previews_state_called) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&PreviewsStateResourceDispatcherHostDelegate::
                           CheckResourcesRequested,
                       base::Unretained(delegate_.get()),
                       should_get_previews_state_called));
  }

 private:
  std::unique_ptr<PreviewsStateResourceDispatcherHostDelegate> delegate_;
};

// Test that navigating calls GetPreviewsState with SERVER_LOFI_ON.
IN_PROC_BROWSER_TEST_F(PreviewsStateBrowserTest, ShouldEnableLoFiModeOn) {
  // Navigate with ShouldEnableLoFiMode returning true.
  Reset(SERVER_LOFI_ON);
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html"), 1);
  CheckResourcesRequested(true);
}

// Test that navigating calls GetPreviewsState returning PREVIEWS_OFF.
IN_PROC_BROWSER_TEST_F(PreviewsStateBrowserTest, ShouldEnableLoFiModeOff) {
  // Navigate with GetPreviewsState returning false.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html"), 1);
  CheckResourcesRequested(true);
}

// Test that reloading calls GetPreviewsState again and changes the Previews
// state.
IN_PROC_BROWSER_TEST_F(PreviewsStateBrowserTest, ShouldEnableLoFiModeReload) {
  // Navigate with GetPreviewsState returning PREVIEWS_OFF.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html"), 1);
  CheckResourcesRequested(true);

  // Reload. GetPreviewsState should be called.
  Reset(SERVER_LOFI_ON);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  CheckResourcesRequested(true);
}

// Test that navigating backwards calls GetPreviewsState again and changes
// the Previews state.
IN_PROC_BROWSER_TEST_F(PreviewsStateBrowserTest,
                       ShouldEnableLoFiModeNavigateBackThenForward) {
  // Navigate with GetPreviewsState returning false.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html"), 1);
  CheckResourcesRequested(true);

  // Go to a different page.
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);

  // Go back with GetPreviewsState returning SERVER_LOFI_ON.
  Reset(SERVER_LOFI_ON);
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  shell()->GoBackOrForward(-1);
  tab_observer.Wait();
  CheckResourcesRequested(true);
}

namespace {

struct RequestData {
  const GURL url;
  const GURL first_party;
  const base::Optional<url::Origin> initiator;
  const int load_flags;
  const std::string referrer;

  RequestData(const GURL& url,
              const GURL& first_party,
              const base::Optional<url::Origin>& initiator,
              int load_flags,
              const std::string& referrer)
      : url(url),
        first_party(first_party),
        initiator(initiator),
        load_flags(load_flags),
        referrer(referrer) {}
};

const GURL kURLWithUniqueOrigin("data:,");

}  // namespace

class RequestDataBrowserTest : public ContentBrowserTest {
 public:
  RequestDataBrowserTest()
      : interceptor_(std::make_unique<content::URLLoaderInterceptor>(
            base::BindRepeating(&RequestDataBrowserTest::OnRequest,
                                base::Unretained(this)))) {}
  ~RequestDataBrowserTest() override {}

  std::vector<RequestData> data() {
    base::AutoLock auto_lock(requests_lock_);
    auto copy = requests_;
    return copy;
  }

  void WaitForRequests(size_t count) {
    while (true) {
      base::RunLoop run_loop;
      {
        base::AutoLock auto_lock(requests_lock_);
        if (requests_.size() == count)
          return;
        requests_closure_ = run_loop.QuitClosure();
      }
      run_loop.Run();
    }
  }

 private:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override { interceptor_.reset(); }

  bool OnRequest(URLLoaderInterceptor::RequestParams* params) {
    RequestCreated(RequestData(
        params->url_request.url, params->url_request.site_for_cookies,
        params->url_request.request_initiator, params->url_request.load_flags,
        params->url_request.referrer.spec()));
    return false;
  }

  void RequestCreated(RequestData data) {
    base::AutoLock auto_lock(requests_lock_);
    requests_.push_back(data);
    if (requests_closure_)
      requests_closure_.Run();
  }

  base::Lock requests_lock_;
  std::vector<RequestData> requests_;
  base::Closure requests_closure_;
  std::unique_ptr<URLLoaderInterceptor> interceptor_;
};

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, Basic) {
  GURL top_url(embedded_test_server()->GetURL("/page_with_subresources.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  auto requests = data();
  EXPECT_EQ(8u, requests.size());

  // All resources loaded directly by the top-level document should have a
  // |first_party| and |initiator| that match the URL of the top-level document.
  // The top-level document itself doesn't have an |initiator|.
  const RequestData* first_request = &requests[0];
  EXPECT_EQ(top_url, first_request->first_party);
  EXPECT_FALSE(first_request->initiator.has_value());
  for (size_t i = 1; i < requests.size(); i++) {
    const RequestData* request = &requests[i];
    EXPECT_EQ(top_url, request->first_party);
    ASSERT_TRUE(request->initiator.has_value());
    EXPECT_EQ(top_origin, request->initiator);
  }
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, LinkRelPrefetch) {
  GURL top_url(embedded_test_server()->GetURL("/link_rel_prefetch.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);
  WaitForRequests(2u);

  auto requests = data();
  EXPECT_EQ(2u, requests.size());
  auto* request = &requests[1];
  EXPECT_EQ(top_origin, request->initiator);
  EXPECT_EQ(top_url, request->referrer);
  EXPECT_TRUE(request->load_flags & net::LOAD_PREFETCH);
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, LinkRelPrefetchReferrerPolicy) {
  GURL top_url(embedded_test_server()->GetURL(
      "/link_rel_prefetch_referrer_policy.html"));
  GURL img_url(embedded_test_server()->GetURL("/image.jpg"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);
  WaitForRequests(2u);

  auto requests = data();
  EXPECT_EQ(2u, requests.size());
  auto* main_frame_request = &requests[0];
  auto* image_request = &requests[1];

  // Check the main frame request.
  EXPECT_EQ(top_url, main_frame_request->url);
  EXPECT_FALSE(main_frame_request->initiator.has_value());

  // Check the image request.
  EXPECT_EQ(img_url, image_request->url);
  EXPECT_TRUE(image_request->initiator.has_value());
  EXPECT_EQ(top_origin, image_request->initiator);
  // Respect the "origin" policy set by the <meta> tag.
  EXPECT_EQ(top_url.GetOrigin().spec(), image_request->referrer);
  EXPECT_TRUE(image_request->load_flags & net::LOAD_PREFETCH);
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, BasicCrossSite) {
  GURL top_url(embedded_test_server()->GetURL(
      "a.com", "/nested_page_with_subresources.html"));
  GURL nested_url(embedded_test_server()->GetURL(
      "not-a.com", "/page_with_subresources.html"));
  url::Origin top_origin = url::Origin::Create(top_url);
  url::Origin nested_origin = url::Origin::Create(nested_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  auto requests = data();
  EXPECT_EQ(9u, requests.size());

  // The first items loaded are the top-level and nested documents. These should
  // both have a |first_party| that match the URL of the top-level document.
  // The top-level document has no initiator and the nested frame is initiated
  // by the top-level document.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_EQ(top_url, requests[0].first_party);
  EXPECT_FALSE(requests[0].initiator.has_value());

  EXPECT_EQ(nested_url, requests[1].url);
  EXPECT_EQ(top_url, requests[1].first_party);
  EXPECT_EQ(top_origin, requests[1].initiator);

  // The remaining items are loaded as subresources in the nested document, and
  // should have a unique first-party, and an initiator that matches the
  // document in which they're embedded.
  for (size_t i = 2; i < requests.size(); i++) {
    SCOPED_TRACE(requests[i].url);
    EXPECT_EQ(kURLWithUniqueOrigin, requests[i].first_party);
    EXPECT_EQ(nested_origin, requests[i].initiator);
  }
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, SameOriginNested) {
  GURL top_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  GURL image_url(embedded_test_server()->GetURL("/image.jpg"));
  GURL nested_url(embedded_test_server()->GetURL("/title1.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  auto requests = data();
  EXPECT_EQ(3u, requests.size());

  // User-initiated top-level navigations have a first-party that matches the
  // URL to which they navigate. The navigation was initiated outside of a
  // document, so there is no |initiator|.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_EQ(top_url, requests[0].first_party);
  EXPECT_FALSE(requests[0].initiator.has_value());

  // Subresource requests have a first-party and initiator that matches the
  // document in which they're embedded.
  EXPECT_EQ(image_url, requests[1].url);
  EXPECT_EQ(top_url, requests[1].first_party);
  EXPECT_EQ(top_origin, requests[1].initiator);

  // Same-origin nested frames have a first-party and initiator that matches
  // the document in which they're embedded.
  EXPECT_EQ(nested_url, requests[2].url);
  EXPECT_EQ(top_url, requests[2].first_party);
  EXPECT_EQ(top_origin, requests[2].initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, SameOriginAuxiliary) {
  GURL top_url(embedded_test_server()->GetURL("/simple_links.html"));
  GURL auxiliary_url(embedded_test_server()->GetURL("/title2.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteNewWindowLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  WaitForLoadStop(new_shell->web_contents());

  auto requests = data();
  EXPECT_EQ(2u, requests.size());

  // User-initiated top-level navigations have a first-party that matches the
  // URL to which they navigate, even if they fail to load. The navigation was
  // initiated outside of a document, so there is no |initiator|.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_EQ(top_url, requests[0].first_party);
  EXPECT_FALSE(requests[0].initiator.has_value());

  // Auxiliary navigations have a first-party that matches the URL to which they
  // navigate, and an initiator that matches the document that triggered them.
  EXPECT_EQ(auxiliary_url, requests[1].url);
  EXPECT_EQ(auxiliary_url, requests[1].first_party);
  EXPECT_EQ(top_origin, requests[1].initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, CrossOriginAuxiliary) {
  GURL top_url(embedded_test_server()->GetURL("/simple_links.html"));
  GURL auxiliary_url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  const char kReplacePortNumber[] =
      "window.domAutomationController.send(setPortNumber(%d));";
  uint16_t port_number = embedded_test_server()->port();
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), base::StringPrintf(kReplacePortNumber, port_number), &success));
  success = false;

  ShellAddedObserver new_shell_observer;
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickCrossSiteNewWindowLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  WaitForLoadStop(new_shell->web_contents());

  auto requests = data();
  EXPECT_EQ(2u, requests.size());

  // User-initiated top-level navigations have a first-party that matches the
  // URL to which they navigate, even if they fail to load. The navigation was
  // initiated outside of a document, so there is no initiator.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_EQ(top_url, requests[0].first_party);
  EXPECT_FALSE(requests[0].initiator.has_value());

  // Auxiliary navigations have a first-party that matches the URL to which they
  // navigate, and an initiator that matches the document that triggered them.
  EXPECT_EQ(auxiliary_url, requests[1].url);
  EXPECT_EQ(auxiliary_url, requests[1].first_party);
  EXPECT_EQ(top_origin, requests[1].initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, FailedNavigation) {
  // Navigating to this URL will fail, as we haven't taught the host resolver
  // about 'a.com'.
  GURL top_url(embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  url::Origin top_origin = url::Origin::Create(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  auto requests = data();
  EXPECT_EQ(1u, requests.size());

  // User-initiated top-level navigations have a first-party that matches the
  // URL to which they navigate, even if they fail to load. The navigation was
  // initiated outside of a document, so there is no initiator.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_EQ(top_url, requests[0].first_party);
  EXPECT_FALSE(requests[0].initiator.has_value());
}

IN_PROC_BROWSER_TEST_F(RequestDataBrowserTest, CrossOriginNested) {
  GURL top_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL top_js_url(
      embedded_test_server()->GetURL("a.com", "/tree_parser_util.js"));
  GURL nested_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b()"));
  GURL nested_js_url(
      embedded_test_server()->GetURL("b.com", "/tree_parser_util.js"));
  url::Origin top_origin = url::Origin::Create(top_url);
  url::Origin nested_origin = url::Origin::Create(nested_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  auto requests = data();
  EXPECT_EQ(4u, requests.size());

  // User-initiated top-level navigations have a |first-party|. The navigation
  // was initiated outside of a document, so there are no initiator.
  EXPECT_EQ(top_url, requests[0].url);
  EXPECT_EQ(top_url, requests[0].first_party);
  EXPECT_FALSE(requests[0].initiator.has_value());

  EXPECT_EQ(top_js_url, requests[1].url);
  EXPECT_EQ(top_url, requests[1].first_party);
  EXPECT_EQ(top_origin, requests[1].initiator);

  // Cross-origin frames have a first-party and initiator that matches the URL
  // in which they're embedded.
  EXPECT_EQ(nested_url, requests[2].url);
  EXPECT_EQ(top_url, requests[2].first_party);
  EXPECT_EQ(top_origin, requests[2].initiator);

  // Cross-origin subresource requests have a unique first-party, and an
  // initiator that matches the document in which they're embedded.
  EXPECT_EQ(nested_js_url, requests[3].url);
  EXPECT_EQ(kURLWithUniqueOrigin, requests[3].first_party);
  EXPECT_EQ(nested_origin, requests[3].initiator);
}

// Regression test for https://crbug.com/648608. An attacker could trivially
// bypass cookies SameSite=Strict protections by navigating a new window twice.
IN_PROC_BROWSER_TEST_F(LoaderBrowserTest,
                       CookieSameSiteStrictOpenNewNamedWindowTwice) {
  // TODO(lukasza): https://crbug.com/417518: Get tests working with
  // --site-per-process.
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Add cookies for 'a.com', one of them with the "SameSite=Strict" option.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  GURL a_url("http://a.com");
  EXPECT_TRUE(SetCookie(context, a_url, "cookie_A=A; SameSite=Strict;"));
  EXPECT_TRUE(SetCookie(context, a_url, "cookie_B=B"));

  // 2) Navigate to malicious.com.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "malicious.com", "/title1.html")));

  // 2.1) malicious.com opens a new window to 'http://a.com/echoall'.
  GURL echoall_url = embedded_test_server()->GetURL("a.com", "/echoall");
  std::string script = base::StringPrintf("window.open('%s', 'named_frame');",
                                          echoall_url.spec().c_str());
  {
    TestNavigationObserver new_tab_observer(shell()->web_contents(), 1);
    new_tab_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecuteScript(shell(), script));
    new_tab_observer.Wait();
    ASSERT_EQ(2u, Shell::windows().size());
    Shell* new_shell = Shell::windows()[1];
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    // Only the cookie without "SameSite=Strict" should be sent.
    std::string html_content;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        new_shell, "domAutomationController.send(document.body.textContent)",
        &html_content));
    EXPECT_THAT(html_content.c_str(), Not(HasSubstr("cookie_A=A")));
    EXPECT_THAT(html_content.c_str(), HasSubstr("cookie_B=B"));
  }

  // 2.2) Same as in 2.1). The difference is that the new tab will be reused.
  {
    Shell* new_shell = Shell::windows()[1];
    TestNavigationObserver new_tab_observer(new_shell->web_contents(), 1);
    EXPECT_TRUE(ExecuteScript(shell(), script));
    new_tab_observer.Wait();
    ASSERT_EQ(2u, Shell::windows().size());
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    // Only the cookie without "SameSite=Strict" should be sent.
    std::string html_content;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        new_shell, "domAutomationController.send(document.body.textContent)",
        &html_content));
    EXPECT_THAT(html_content.c_str(), Not(HasSubstr("cookie_A=A")));
    EXPECT_THAT(html_content.c_str(), HasSubstr("cookie_B=B"));
  }
}

}  // namespace content
