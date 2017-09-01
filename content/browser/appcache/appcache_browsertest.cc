// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <set>
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/appcache/appcache_subresource_url_factory.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
namespace content {

// This class currently enables the network service feature, which allows us to
// test the AppCache code in that mode.
class AppCacheNetworkServiceBrowserTest : public ContentBrowserTest {
 public:
  AppCacheNetworkServiceBrowserTest() {}

  // Handler to count the number of requests.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    request_count_++;

    relative_urls_seen_.insert(request.relative_url);

    // Generate a redirect response for this resource.
    if (!base::CompareCaseInsensitiveASCII(request.relative_url,
                                           "/appcache/redirect/logo.png")) {
      std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
          new net::test_server::BasicHttpResponse);
      http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      std::string dest_url = request.base_url.spec();
      dest_url += "appcache/logo.png";
      http_response->AddCustomHeader("Location", dest_url.c_str());
      redirect_count_++;
      return std::move(http_response);
    }
    return std::unique_ptr<net::test_server::HttpResponse>();
  }

  // Call this to reset the request_count_.
  void Clear() {
    request_count_ = 0;
    redirect_count_ = 0;
    relative_urls_seen_.clear();
  }

  int request_count() const { return request_count_; }

  int redirect_count() const { return redirect_count_; }

  bool SeenRelativeUrl(const std::string& url) {
    return relative_urls_seen_.find(url) != relative_urls_seen_.end();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    features::kNetworkService.name);
  }

 private:
  // Tracks the number of requests.
  int request_count_ = 0;
  int redirect_count_ = 0;

  // Holds the list of relative URLs seen by the HTTP request handler.
  std::set<std::string> relative_urls_seen_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheNetworkServiceBrowserTest);
};

// The network service process launch DCHECK's on Android. The bug
// here http://crbug.com/748764. It looks like unsandboxed utility
// processes are not supported on Android.
#if !defined(OS_ANDROID)
// This test validates that navigating to a TLD which has an AppCache
// associated with it and then navigating to another TLD within that
// host clears the previously registered factory. We verify this by
// validating that request count for the last navigation.
IN_PROC_BROWSER_TEST_F(AppCacheNetworkServiceBrowserTest,
                       VerifySubresourceFactoryClearedOnNewNavigation) {
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server(
      new net::EmbeddedTestServer());

  embedded_test_server->RegisterRequestHandler(
      base::Bind(&AppCacheNetworkServiceBrowserTest::HandleRequest,
                 base::Unretained(this)));

  base::FilePath content_test_data(FILE_PATH_LITERAL("content/test/data"));
  embedded_test_server->AddDefaultHandlers(content_test_data);

  ASSERT_TRUE(embedded_test_server->Start());

  GURL main_url =
      embedded_test_server->GetURL("/appcache/simple_page_with_manifest.html");

  base::string16 expected_title = base::ASCIIToUTF16("AppCache updated");

  // Load the main page twice. The second navigation should have AppCache
  // initialized for the page.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(main_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  Clear();
  GURL page_no_manifest =
      embedded_test_server->GetURL("/appcache/simple_page_no_manifest.html");

  EXPECT_TRUE(NavigateToURL(shell(), page_no_manifest));
  // We expect two requests for simple_page_no_manifest.html. The request
  // for the main page and the logo.
  EXPECT_GT(request_count(), 1);
  EXPECT_EQ(page_no_manifest, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

// This test validates that navigating to a page which has an AppCache manifest
// and subresources which are redirected to resources which exist in the
// AppCache are loaded from the cache.
IN_PROC_BROWSER_TEST_F(AppCacheNetworkServiceBrowserTest,
                       VerifyRedirectedSubresourceAppCacheLoad) {
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server(
      new net::EmbeddedTestServer());

  embedded_test_server->RegisterRequestHandler(
      base::Bind(&AppCacheNetworkServiceBrowserTest::HandleRequest,
                 base::Unretained(this)));

  base::FilePath content_test_data(FILE_PATH_LITERAL("content/test/data"));
  embedded_test_server->AddDefaultHandlers(content_test_data);

  ASSERT_TRUE(embedded_test_server->Start());

  GURL main_url = embedded_test_server->GetURL(
      "/appcache/simple_page_redirected_resource_manifest.html");

  base::string16 expected_title = base::ASCIIToUTF16("AppCache updated");

  // Load the main page twice. The second navigation should have AppCache
  // initialized for the page.
  // We expect to a redirect response and then a request for the redirected
  // resource here.
  TitleWatcher title_watcher_update(shell()->web_contents(), expected_title);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(expected_title, title_watcher_update.WaitAndGetTitle());
  EXPECT_EQ(1, redirect_count());
  EXPECT_TRUE(SeenRelativeUrl("/appcache/redirect/logo.png"));
  EXPECT_TRUE(SeenRelativeUrl("/appcache/logo.png"));

  // The second attempt to load the page should serve the resources out of the
  // cache. Here we expect to see one redirect request. The redirected resource
  // should be served out of the AppCache.
  Clear();
  expected_title = base::ASCIIToUTF16("AppCache image loaded");
  TitleWatcher title_watcher_image_loaded(shell()->web_contents(),
                                          expected_title);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, redirect_count());
  EXPECT_TRUE(SeenRelativeUrl("/appcache/redirect/logo.png"));
  EXPECT_FALSE(SeenRelativeUrl("/appcache/logo.png"));
  EXPECT_EQ(expected_title, title_watcher_image_loaded.WaitAndGetTitle());
}

#endif

}  // namespace content