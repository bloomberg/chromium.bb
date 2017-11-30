// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/test_resource_handler.h"
#include "content/browser/loader_delegate_impl.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_url_request_job.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_navigation_url_loader_delegate.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

class StreamProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  StreamProtocolHandler(StreamRegistry* registry) : registry_(registry) {}

  // net::URLRequestJobFactory::ProtocolHandler implementation.
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    scoped_refptr<Stream> stream = registry_->GetStream(request->url());
    if (stream.get())
      return new StreamURLRequestJob(request, network_delegate, stream);
    return nullptr;
  }

 private:
  StreamRegistry* registry_;

  DISALLOW_COPY_AND_ASSIGN(StreamProtocolHandler);
};

class DownloadProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  DownloadProtocolHandler() {}

  // net::URLRequestJobFactory::ProtocolHandler implementation.
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    static const char kHeaders[] =
        "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n"
        "Content-disposition: attachment\n"
        "\n";
    return new net::URLRequestTestJob(request, network_delegate, kHeaders,
                                      "test", false);
  }

  DISALLOW_COPY_AND_ASSIGN(DownloadProtocolHandler);
};

class RequestBlockingResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  // ResourceDispatcherHostDelegate implementation:
  bool ShouldBeginRequest(const std::string& method,
                          const GURL& url,
                          ResourceType resource_type,
                          ResourceContext* resource_context) override {
    return false;
  }
};

std::unique_ptr<ResourceHandler> CreateDownloadResourceHandler(
    net::URLRequest* request) {
  return std::make_unique<TestResourceHandler>();
}

}  // namespace

class NavigationURLLoaderTest : public testing::Test {
 public:
  NavigationURLLoaderTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext),
        host_(base::Bind(&CreateDownloadResourceHandler),
              base::ThreadTaskRunnerHandle::Get(),
              /* enable_resource_scheduler */ true) {
    host_.SetLoaderDelegate(&loader_delegate_);
    BrowserContext::EnsureResourceContextInitialized(browser_context_.get());
    base::RunLoop().RunUntilIdle();
    net::URLRequestContext* request_context =
        browser_context_->GetResourceContext()->GetRequestContext();
    // Attach URLRequestTestJob and make streams work.
    job_factory_.SetProtocolHandler(
        "test", net::URLRequestTestJob::CreateProtocolHandler());
    job_factory_.SetProtocolHandler(
        "blob", std::make_unique<StreamProtocolHandler>(
                    StreamContext::GetFor(browser_context_.get())->registry()));
    job_factory_.SetProtocolHandler(
        "download", std::make_unique<DownloadProtocolHandler>());
    request_context->set_job_factory(&job_factory_);

    // NavigationURLLoader is only used for browser-side navigations.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableBrowserSideNavigation);
  }

  std::unique_ptr<NavigationURLLoader> MakeTestLoader(
      const GURL& url,
      NavigationURLLoaderDelegate* delegate) {
    return CreateTestLoader(url, delegate, false);
  }

  std::unique_ptr<NavigationURLLoader> CreateTestLoader(
      const GURL& url,
      NavigationURLLoaderDelegate* delegate,
      bool allow_download) {
    BeginNavigationParams begin_params(
        std::string(), net::LOAD_NORMAL, false, REQUEST_CONTEXT_TYPE_LOCATION,
        blink::WebMixedContentContextType::kBlockable,
        false,  // is_form_submission
        url::Origin::Create(url));
    CommonNavigationParams common_params;
    common_params.url = url;
    common_params.allow_download = allow_download;

    std::unique_ptr<NavigationRequestInfo> request_info(
        new NavigationRequestInfo(common_params, begin_params, url, true, false,
                                  false, -1, false, false,
                                  blink::mojom::PageVisibilityState::kVisible));
    return NavigationURLLoader::Create(
        browser_context_->GetResourceContext(),
        BrowserContext::GetDefaultStoragePartition(browser_context_.get()),
        std::move(request_info), nullptr, nullptr, nullptr, delegate);
  }

  // Helper function for fetching the body of a URL to a string.
  std::string FetchURL(const GURL& url) {
    net::TestDelegate delegate;
    net::URLRequestContext* request_context =
        browser_context_->GetResourceContext()->GetRequestContext();
    std::unique_ptr<net::URLRequest> request(request_context->CreateRequest(
        url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    request->Start();
    base::RunLoop().Run();

    EXPECT_TRUE(request->status().is_success());
    EXPECT_EQ(200, request->response_headers()->response_code());
    return delegate.data_received();
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  net::URLRequestJobFactoryImpl job_factory_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  LoaderDelegateImpl loader_delegate_;
  ResourceDispatcherHostImpl host_;
};

// Tests that a basic request works.
TEST_F(NavigationURLLoaderTest, Basic) {
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_1(), &delegate);

  // Wait for the response to come back.
  delegate.WaitForResponseStarted();

  // Proceed with the response.
  loader->ProceedWithResponse();

  // Check the response is correct.
  EXPECT_EQ("text/html", delegate.response()->head.mime_type);
  EXPECT_EQ(200, delegate.response()->head.headers->response_code());

  // Check the body is correct.
  EXPECT_EQ(net::URLRequestTestJob::test_data_1(),
            FetchURL(delegate.body()->GetURL()));

  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that request failures are propagated correctly.
TEST_F(NavigationURLLoaderTest, RequestFailedNoCertError) {
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      MakeTestLoader(GURL("bogus:bogus"), &delegate);

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_UNKNOWN_URL_SCHEME, delegate.net_error());
  EXPECT_FALSE(delegate.ssl_info().is_valid());
  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that request failures are propagated correctly for a (non-fatal) cert
// error:
// - |ssl_info| has the expected values.
// - |should_ssl_errors_be_fatal| is false.
TEST_F(NavigationURLLoaderTest, RequestFailedCertError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(https_server.Start());

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      MakeTestLoader(https_server.GetURL("/"), &delegate);

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  ASSERT_EQ(net::ERR_ABORTED, delegate.net_error());
  net::SSLInfo ssl_info = delegate.ssl_info();
  EXPECT_TRUE(ssl_info.is_valid());
  EXPECT_TRUE(https_server.GetCertificate()->Equals(ssl_info.cert.get()));
  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID,
            net::MapCertStatusToNetError(ssl_info.cert_status));
  EXPECT_FALSE(delegate.should_ssl_errors_be_fatal());
  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that request failures are propagated correctly for a fatal cert error:
// - |ssl_info| has the expected values.
// - |should_ssl_errors_be_fatal| is true.
TEST_F(NavigationURLLoaderTest, RequestFailedCertErrorFatal) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(https_server.Start());
  GURL url = https_server.GetURL("/");

  // Set HSTS for the test domain in order to make SSL errors fatal.
  net::TransportSecurityState* transport_security_state =
      browser_context_->GetResourceContext()
          ->GetRequestContext()
          ->transport_security_state();
  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  bool include_subdomains = false;
  transport_security_state->AddHSTS(url.host(), expiry, include_subdomains);

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = MakeTestLoader(url, &delegate);

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  ASSERT_EQ(net::ERR_ABORTED, delegate.net_error());
  net::SSLInfo ssl_info = delegate.ssl_info();
  EXPECT_TRUE(ssl_info.is_valid());
  EXPECT_TRUE(https_server.GetCertificate()->Equals(ssl_info.cert.get()));
  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID,
            net::MapCertStatusToNetError(ssl_info.cert_status));
  EXPECT_TRUE(delegate.should_ssl_errors_be_fatal());
  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Test that redirects are sent to the delegate.
TEST_F(NavigationURLLoaderTest, RequestRedirected) {
  // Fake a top-level request. Choose a URL which redirects so the request can
  // be paused before the response comes in.
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = MakeTestLoader(
      net::URLRequestTestJob::test_url_redirect_to_url_2(), &delegate);

  // Wait for the request to redirect.
  delegate.WaitForRequestRedirected();
  EXPECT_EQ(net::URLRequestTestJob::test_url_2(),
            delegate.redirect_info().new_url);
  EXPECT_EQ("GET", delegate.redirect_info().new_method);
  EXPECT_EQ(net::URLRequestTestJob::test_url_2(),
            delegate.redirect_info().new_site_for_cookies);
  EXPECT_EQ(302, delegate.redirect_response()->head.headers->response_code());
  EXPECT_EQ(1, delegate.on_request_handled_counter());

  // Wait for the response to complete.
  loader->FollowRedirect();
  delegate.WaitForResponseStarted();

  // Proceed with the response.
  loader->ProceedWithResponse();

  // Check the response is correct.
  EXPECT_EQ("text/html", delegate.response()->head.mime_type);
  EXPECT_EQ(200, delegate.response()->head.headers->response_code());

  // Release the body and check it is correct.
  EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());
  EXPECT_EQ(net::URLRequestTestJob::test_data_2(),
            FetchURL(delegate.body()->GetURL()));

  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that the destroying the loader cancels the request.
TEST_F(NavigationURLLoaderTest, CancelOnDestruct) {
  // Fake a top-level request. Choose a URL which redirects so the request can
  // be paused before the response comes in.
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = MakeTestLoader(
      net::URLRequestTestJob::test_url_redirect_to_url_2(), &delegate);

  // Wait for the request to redirect.
  delegate.WaitForRequestRedirected();

  // Destroy the loader and verify that URLRequestTestJob no longer has anything
  // paused.
  loader.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(net::URLRequestTestJob::ProcessOnePendingMessage());
}

// Test that the delegate is not called if OnResponseStarted and destroying the
// loader race.
TEST_F(NavigationURLLoaderTest, CancelResponseRace) {
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = MakeTestLoader(
      net::URLRequestTestJob::test_url_redirect_to_url_2(), &delegate);

  // Wait for the request to redirect.
  delegate.WaitForRequestRedirected();

  // In the same event loop iteration, follow the redirect (allowing the
  // response to go through) and destroy the loader.
  loader->FollowRedirect();
  loader.reset();

  // Verify the URLRequestTestJob no longer has anything paused and that no
  // response body was received.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(net::URLRequestTestJob::ProcessOnePendingMessage());
  EXPECT_FALSE(delegate.body());
}

// Tests that the loader may be canceled by context.
TEST_F(NavigationURLLoaderTest, CancelByContext) {
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = MakeTestLoader(
      net::URLRequestTestJob::test_url_redirect_to_url_2(), &delegate);

  // Wait for the request to redirect.
  delegate.WaitForRequestRedirected();

  // Cancel all requests.
  host_.CancelRequestsForContext(browser_context_->GetResourceContext());

  // Wait for the request to now be aborted.
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_ABORTED, delegate.net_error());
  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that, if the request is blocked by the ResourceDispatcherHostDelegate,
// the caller is informed appropriately.
TEST_F(NavigationURLLoaderTest, RequestBlocked) {
  RequestBlockingResourceDispatcherHostDelegate rdh_delegate;
  host_.SetDelegate(&rdh_delegate);

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_1(), &delegate);

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_ABORTED, delegate.net_error());

  // Failing before start means OnRequestStarted is never called.
  EXPECT_EQ(0, delegate.on_request_handled_counter());

  host_.SetDelegate(nullptr);
}

// Tests that ownership leaves the loader once the response is received.
TEST_F(NavigationURLLoaderTest, LoaderDetached) {
  // Fake a top-level request to a URL whose body does not load immediately.
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_2(), &delegate);

  // Wait for the response to come back.
  delegate.WaitForResponseStarted();

  // Proceed with the response.
  loader->ProceedWithResponse();

  // Check the response is correct.
  EXPECT_EQ("text/html", delegate.response()->head.mime_type);
  EXPECT_EQ(200, delegate.response()->head.headers->response_code());

  // Destroy the loader.
  loader.reset();
  base::RunLoop().RunUntilIdle();

  // Check the body can still be fetched through the StreamHandle.
  EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());
  EXPECT_EQ(net::URLRequestTestJob::test_data_2(),
            FetchURL(delegate.body()->GetURL()));
}

// Tests that the request is owned by the body StreamHandle.
TEST_F(NavigationURLLoaderTest, OwnedByHandle) {
  // Fake a top-level request to a URL whose body does not load immediately.
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_2(), &delegate);

  // Wait for the response to come back.
  delegate.WaitForResponseStarted();

  // Proceed with the response.
  loader->ProceedWithResponse();

  // Release the body.
  delegate.ReleaseBody();
  base::RunLoop().RunUntilIdle();

  // Verify that URLRequestTestJob no longer has anything paused.
  EXPECT_FALSE(net::URLRequestTestJob::ProcessOnePendingMessage());
}

// Tests that download is allowed for the request.
TEST_F(NavigationURLLoaderTest, DownloadAllowed) {
  // Fake a top-level request to a URL whose body does not load immediately.
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      CreateTestLoader(GURL("download:test"), &delegate, true);

  delegate.WaitForResponseStarted();
  EXPECT_TRUE(delegate.is_download());
  loader.reset();
  base::RunLoop().RunUntilIdle();
}

// Tests that download is not allowed for the request.
TEST_F(NavigationURLLoaderTest, DownloadDisallowed) {
  // Fake a top-level request to a URL whose body does not load immediately.
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      CreateTestLoader(GURL("download:test"), &delegate, false);

  delegate.WaitForResponseStarted();
  EXPECT_FALSE(delegate.is_download());
  loader.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
