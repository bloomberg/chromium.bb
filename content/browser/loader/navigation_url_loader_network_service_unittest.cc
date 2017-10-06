// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_network_service.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/common/navigation_params.h"
#include "content/common/service_manager/service_manager_connection_impl.h"
#include "content/network/network_context.h"
#include "content/network/url_loader_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_navigation_url_loader_delegate.h"
#include "net/base/load_flags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
class TestURLLoaderRequestHandler : public URLLoaderRequestHandler {
 public:
  explicit TestURLLoaderRequestHandler(
      base::Optional<ResourceRequest>* most_recent_resource_request)
      : most_recent_resource_request_(most_recent_resource_request),
        context_(NetworkContext::CreateForTesting()) {}
  ~TestURLLoaderRequestHandler() override {}

  void MaybeCreateLoader(const ResourceRequest& resource_request,
                         ResourceContext* resource_context,
                         LoaderCallback callback) override {
    std::move(callback).Run(
        base::Bind(&TestURLLoaderRequestHandler::StartLoader,
                   base::Unretained(this), resource_request));
  }

  void StartLoader(ResourceRequest resource_request,
                   mojom::URLLoaderRequest request,
                   mojom::URLLoaderClientPtr client) {
    *most_recent_resource_request_ = resource_request;
    // The URLLoaderImpl will delete itself upon completion.
    new URLLoaderImpl(context_.get(), std::move(request), 0 /* options */,
                      resource_request, false /* report_raw_headers */,
                      std::move(client), TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  mojom::URLLoaderFactoryPtr MaybeCreateSubresourceFactory() override {
    return nullptr;
  }

  bool MaybeCreateLoaderForResponse(
      const ResourceResponseHead& response,
      mojom::URLLoaderPtr* loader,
      mojom::URLLoaderClientRequest* client_request) override {
    return false;
  }

 private:
  base::Optional<ResourceRequest>* most_recent_resource_request_;  // NOT OWNED.
  std::unique_ptr<NetworkContext> context_;
};

}  // namespace

class NavigationURLLoaderNetworkServiceTest : public testing::Test {
 public:
  NavigationURLLoaderNetworkServiceTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
    // NavigationURLLoader is only used for browser-side navigations.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableBrowserSideNavigation);
    feature_list_.InitAndEnableFeature(features::kNetworkService);

    // Because the network service is enabled we need a ServiceManagerConnection
    // or BrowserContext::GetDefaultStoragePartition will segfault when
    // ContentBrowserClient::CreateNetworkContext tries to call
    // GetNetworkService.
    service_manager::mojom::ServicePtr service;
    ServiceManagerConnection::SetForProcess(
        base::MakeUnique<ServiceManagerConnectionImpl>(
            mojo::MakeRequest(&service),
            BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)));

    browser_context_.reset(new TestBrowserContext);
    http_test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  }

  ~NavigationURLLoaderNetworkServiceTest() override {
    ServiceManagerConnection::DestroyForProcess();
  }

  std::unique_ptr<NavigationURLLoader> CreateTestLoader(
      const GURL& url,
      const std::string& headers,
      const std::string& method,
      NavigationURLLoaderDelegate* delegate,
      bool allow_download = false) {
    BeginNavigationParams begin_params(
        headers, net::LOAD_NORMAL, false /* has_user_gesture */,
        false /* skip_service_worker */, REQUEST_CONTEXT_TYPE_LOCATION,
        blink::WebMixedContentContextType::kBlockable,
        false /* is_form_submission */, url::Origin(url));

    CommonNavigationParams common_params;
    common_params.url = url;
    common_params.method = method;
    common_params.allow_download = allow_download;

    std::unique_ptr<NavigationRequestInfo> request_info(
        new NavigationRequestInfo(
            common_params, begin_params, url, true /* is_main_frame */,
            false /* parent_is_main_frame */, false /* are_ancestors_secure */,
            -1 /* frame_tree_node_id */, false /* is_for_guests_only */,
            false /* report_raw_headers */,
            blink::kWebPageVisibilityStateVisible));

    std::vector<std::unique_ptr<URLLoaderRequestHandler>> handlers;
    most_recent_resource_request_ = base::nullopt;
    handlers.push_back(base::MakeUnique<TestURLLoaderRequestHandler>(
        &most_recent_resource_request_));

    return base::MakeUnique<NavigationURLLoaderNetworkService>(
        browser_context_->GetResourceContext(),
        BrowserContext::GetDefaultStoragePartition(browser_context_.get()),
        std::move(request_info), nullptr /* navigation_ui_data */,
        nullptr /* service_worker_handle */, nullptr /* appcache_handle */,
        delegate, std::move(handlers));
  }

  // Requests |redirect_url|, which must return a HTTP 3xx redirect. It's also
  // used as the initial origin.
  // |request_method| is the method to use for the initial request.
  // |expected_redirect_method| is the method that is expected to be used for
  // the second request, after redirection.
  // |expected_origin_value| is the expected value for the Origin header after
  // redirection. If empty, expects that there will be no Origin header.
  void HTTPRedirectOriginHeaderTest(const GURL& redirect_url,
                                    const std::string& request_method,
                                    const std::string& expected_redirect_method,
                                    const std::string& expected_origin_value,
                                    bool expect_request_fail = false) {
    TestNavigationURLLoaderDelegate delegate;
    std::unique_ptr<NavigationURLLoader> loader = CreateTestLoader(
        redirect_url,
        base::StringPrintf("%s: %s", net::HttpRequestHeaders::kOrigin,
                           redirect_url.GetOrigin().spec().c_str()),
        request_method, &delegate);
    delegate.WaitForRequestRedirected();
    loader->FollowRedirect();

    EXPECT_EQ(expected_redirect_method, delegate.redirect_info().new_method);

    if (expect_request_fail) {
      delegate.WaitForRequestFailed();
    } else {
      delegate.WaitForResponseStarted();
    }
    ASSERT_TRUE(most_recent_resource_request_);

    // Note that there is no check for request success here because, for
    // purposes of testing, the request very well may fail. For example, if the
    // test redirects to an HTTPS server from an HTTP origin, thus it is cross
    // origin, there is not an HTTPS server in this unit test framework, so the
    // request would fail. However, that's fine, as long as the request headers
    // are in order and pass the checks below.
    if (expected_origin_value.empty()) {
      EXPECT_FALSE(most_recent_resource_request_->headers.HasHeader(
          net::HttpRequestHeaders::kOrigin));
    } else {
      std::string origin_header;
      EXPECT_TRUE(most_recent_resource_request_->headers.GetHeader(
          net::HttpRequestHeaders::kOrigin, &origin_header));
      EXPECT_EQ(expected_origin_value, origin_header);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  net::EmbeddedTestServer http_test_server_;
  base::Optional<ResourceRequest> most_recent_resource_request_;
};

TEST_F(NavigationURLLoaderNetworkServiceTest, Redirect301Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect301-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect301-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET", std::string(),
                               true);
}

TEST_F(NavigationURLLoaderNetworkServiceTest, Redirect302Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect302-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect302-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET", std::string(),
                               true);
}

TEST_F(NavigationURLLoaderNetworkServiceTest, Redirect303Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect303-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect303-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET", std::string(),
                               true);
}

TEST_F(NavigationURLLoaderNetworkServiceTest, Redirect307Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect307-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect307-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "POST", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null",
                               true);
}

TEST_F(NavigationURLLoaderNetworkServiceTest, Redirect308Tests) {
  ASSERT_TRUE(http_test_server_.Start());

  const GURL url = http_test_server_.GetURL("/redirect308-to-echo");
  const GURL https_redirect_url =
      http_test_server_.GetURL("/redirect308-to-https");

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null", true);
  HTTPRedirectOriginHeaderTest(url, "POST", "POST", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null",
                               true);
}

}  // namespace content
