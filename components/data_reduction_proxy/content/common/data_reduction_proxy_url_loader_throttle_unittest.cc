// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/common/data_reduction_proxy_url_loader_throttle.h"

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/common/previews_state.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class MockMojoDataReductionProxy : public mojom::DataReductionProxy {
 public:
  MockMojoDataReductionProxy() = default;
  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override {
    mark_proxies_as_bad_called++;
    pending_callback = std::move(callback);
  }

  void Clone(mojom::DataReductionProxyRequest request) override {}

  size_t mark_proxies_as_bad_called = 0;
  MarkProxiesAsBadCallback pending_callback;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMojoDataReductionProxy);
};

class MockDelegate : public content::URLLoaderThrottle::Delegate {
 public:
  MockDelegate() = default;

  void CancelWithError(int error_code,
                       base::StringPiece custom_reason = nullptr) override {
    FAIL() << "Should not be reached";
  }

  void Resume() override { resume_called++; }

  void RestartWithFlags(int additional_load_flags) override {
    restart_with_flags_called++;
    restart_additional_load_flags = additional_load_flags;
  }

  size_t resume_called = 0;
  size_t restart_with_flags_called = 0;
  int restart_additional_load_flags = 0;

  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

TEST(DataReductionProxyURLLoaderThrottleTest, AcceptTransformHeaderSet) {
  MockMojoDataReductionProxy drp;
  DataReductionProxyURLLoaderThrottle throttle(net::HttpRequestHeaders(), &drp);
  network::ResourceRequest request;
  request.url = GURL("http://example.com");
  request.resource_type = content::RESOURCE_TYPE_MEDIA;
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  std::string value;
  EXPECT_TRUE(request.custom_proxy_pre_cache_headers.GetHeader(
      chrome_proxy_accept_transform_header(), &value));
  EXPECT_EQ(value, compressed_video_directive());
}

TEST(DataReductionProxyURLLoaderThrottleTest,
     AcceptTransformHeaderSetForMainFrame) {
  MockMojoDataReductionProxy drp;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               &drp);
  network::ResourceRequest request;
  request.url = GURL("http://example.com");
  request.resource_type = content::RESOURCE_TYPE_MAIN_FRAME;
  request.previews_state = content::SERVER_LITE_PAGE_ON;
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  std::string value;
  EXPECT_TRUE(request.custom_proxy_pre_cache_headers.GetHeader(
      chrome_proxy_accept_transform_header(), &value));
  EXPECT_EQ(value, lite_page_directive());
}

TEST(DataReductionProxyURLLoaderThrottleTest,
     ConstructorHeadersAddedToPostCacheHeaders) {
  MockMojoDataReductionProxy drp;
  net::HttpRequestHeaders headers;
  headers.SetHeader("foo", "bar");
  DataReductionProxyURLLoaderThrottle throttle(headers, &drp);
  network::ResourceRequest request;
  request.url = GURL("http://example.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  std::string value;
  EXPECT_TRUE(request.custom_proxy_post_cache_headers.GetHeader("foo", &value));
  EXPECT_EQ(value, "bar");
}

TEST(DataReductionProxyURLLoaderThrottleTest, UseAlternateProxyList) {
  MockMojoDataReductionProxy drp;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               &drp);
  network::ResourceRequest request;
  request.resource_type = content::RESOURCE_TYPE_MEDIA;
  request.url = GURL("http://example.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_TRUE(request.custom_proxy_use_alternate_proxy_list);
}

TEST(DataReductionProxyURLLoaderThrottleTest, DontUseAlternateProxyList) {
  MockMojoDataReductionProxy drp;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               &drp);
  network::ResourceRequest request;
  request.resource_type = content::RESOURCE_TYPE_MAIN_FRAME;
  request.url = GURL("http://example.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(request.custom_proxy_use_alternate_proxy_list);
}

TEST(DataReductionProxyURLLoaderThrottleTest, RestartBypassProxyAndCache) {
  MockMojoDataReductionProxy drp;
  MockDelegate delegate;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               &drp);
  throttle.set_delegate(&delegate);

  auto drp_proxy_server = net::ProxyServer::FromPacString("HTTPS localhost");

  // TODO(https://crbug.com/721403): Inform the throttle of the DRP
  // configuration, which should include |drp_proxy_server|. The test currently
  // passes because of a hack in the throttle.

  network::ResourceRequest request;
  request.resource_type = content::RESOURCE_TYPE_MAIN_FRAME;
  request.url = GURL("http://example.com/");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);

  network::ResourceResponseHead response_head;
  response_head.proxy_server = drp_proxy_server;
  response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head.headers->AddHeader("Chrome-Proxy: block-once");

  // The request should be restarted immediately.
  throttle.BeforeWillProcessResponse(request.url, response_head, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, drp.mark_proxies_as_bad_called);
  EXPECT_EQ(1u, delegate.restart_with_flags_called);
  EXPECT_EQ(net::LOAD_BYPASS_PROXY | net::LOAD_BYPASS_CACHE,
            delegate.restart_additional_load_flags);
}

TEST(DataReductionProxyURLLoaderThrottleTest, DisregardChromeProxyFromDirect) {
  MockMojoDataReductionProxy drp;
  MockDelegate delegate;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               &drp);
  throttle.set_delegate(&delegate);

  // TODO(https://crbug.com/721403): Inform the throttle of the DRP
  // configuration, which should include |drp_proxy_server|.
  auto drp_proxy_server = net::ProxyServer::FromPacString("HTTPS localhost");

  network::ResourceRequest request;
  request.resource_type = content::RESOURCE_TYPE_MAIN_FRAME;
  request.url = GURL("http://example.com/");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);

  // Construct a response that did not come from a proxy server, however
  // includes a Chrome-Proxy header.
  network::ResourceResponseHead response_head;
  response_head.proxy_server = net::ProxyServer::Direct();
  response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head.headers->AddHeader("Chrome-Proxy: block-once");

  throttle.BeforeWillProcessResponse(request.url, response_head, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);
}

TEST(DataReductionProxyURLLoaderThrottleTest, MarkProxyAsBadAndRestart) {
  MockMojoDataReductionProxy drp;
  MockDelegate delegate;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               &drp);
  throttle.set_delegate(&delegate);

  auto drp_proxy_server = net::ProxyServer::FromPacString("HTTPS localhost");

  // TODO(https://crbug.com/721403): Inform the throttle of the DRP
  // configuration, which should include |drp_proxy_server|. The test currently
  // passes because of a hack in the throttle.

  network::ResourceRequest request;
  request.resource_type = content::RESOURCE_TYPE_MAIN_FRAME;
  // TODO(https://crbug.com/721403): Change to to http://example.com/ once
  // config is pushed to throttle. Right now it uses the URL to guess the
  // config.
  request.url = GURL("http://does.not.resolve/echoheader?Chrome-Proxy");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);

  network::ResourceResponseHead response_head;
  response_head.proxy_server = drp_proxy_server;
  response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 500 Server error\n");

  throttle.BeforeWillProcessResponse(request.url, response_head, &defer);

  // The throttle should have marked the proxy as bad.
  EXPECT_TRUE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);
  EXPECT_EQ(1u, drp.mark_proxies_as_bad_called);
  ASSERT_FALSE(drp.pending_callback.is_null());

  // Complete the marking as bad.
  std::move(drp.pending_callback).Run();

  // The throttle should restart and resume.
  EXPECT_EQ(1u, delegate.resume_called);
  EXPECT_EQ(1u, delegate.restart_with_flags_called);
  EXPECT_EQ(0, delegate.restart_additional_load_flags);
  EXPECT_EQ(1u, drp.mark_proxies_as_bad_called);
}

}  // namespace

}  // namespace data_reduction_proxy
