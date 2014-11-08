// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_protocol.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/network_delegate.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_transaction_test_util.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::HttpResponseHeaders;
using net::HostPortPair;
using net::MockRead;
using net::MockWrite;
using net::ProxyRetryInfoMap;
using net::ProxyService;
using net::StaticSocketDataProvider;
using net::TestDelegate;
using net::URLRequest;
using net::TestURLRequestContext;


namespace data_reduction_proxy {

class SimpleURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return net::URLRequestHttpJob::Factory(request, network_delegate, "http");
  }
};

// Constructs a |TestURLRequestContext| that uses a |MockSocketFactory| to
// simulate requests and responses.
class DataReductionProxyProtocolTest : public testing::Test {
 public:
  DataReductionProxyProtocolTest() : http_user_agent_settings_("", "") {
    proxy_params_.reset(
        new TestDataReductionProxyParams(
            DataReductionProxyParams::kAllowed |
            DataReductionProxyParams::kFallbackAllowed |
            DataReductionProxyParams::kPromoAllowed,
            TestDataReductionProxyParams::HAS_EVERYTHING &
            ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
            ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));
    simple_interceptor_.reset(new SimpleURLRequestInterceptor());
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", "www.google.com", simple_interceptor_.Pass());
  }
  ~DataReductionProxyProtocolTest() override {
    // URLRequestJobs may post clean-up tasks on destruction.
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(
            "http", "www.google.com");
    base::RunLoop().RunUntilIdle();
  }

  // Sets up the |TestURLRequestContext| with the provided |ProxyService| and
  // |bypass_type| to store bypass reasons.
  void ConfigureTestDependencies(ProxyService* proxy_service,
                                 DataReductionProxyBypassType* bypass_type) {
    // Create a context with delayed initialization.
    context_.reset(new TestURLRequestContext(true));

    proxy_service_.reset(proxy_service);
    context_->set_client_socket_factory(&mock_socket_factory_);
    context_->set_proxy_service(proxy_service_.get());
    network_delegate_.reset(new net::TestNetworkDelegate());
    context_->set_network_delegate(network_delegate_.get());
    // This is needed to prevent the test context from adding language headers
    // to requests.
    context_->set_http_user_agent_settings(&http_user_agent_settings_);
    usage_stats_.reset(new DataReductionProxyUsageStats(
        proxy_params_.get(), base::MessageLoopProxy::current()));
    DataReductionProxyInterceptor* interceptor =
        new DataReductionProxyInterceptor(proxy_params_.get(),
                                          usage_stats_.get());

    scoped_ptr<net::URLRequestJobFactoryImpl> job_factory_impl(
        new net::URLRequestJobFactoryImpl());
    job_factory_.reset(
        new net::URLRequestInterceptingJobFactory(
            job_factory_impl.Pass(),
            make_scoped_ptr(interceptor)));
    context_->set_job_factory(job_factory_.get());
    context_->Init();
  }

  // Simulates a request to a data reduction proxy that may result in bypassing
  // the proxy and retrying the the request.
  // Runs a test with the given request |method| that expects the first response
  // from the server to be |first_response|. If |expected_retry|, the test
  // will expect a retry of the request. A response body will be expected
  // if |expect_response_body|.
  void TestProxyFallback(const char* method,
                         const char* first_response,
                         bool has_origin,
                         bool expected_retry,
                         size_t expected_bad_proxy_count,
                         bool expect_response_body) {
    std::string payload1 =
        (expected_retry ? "Bypass message" : "content");
    MockRead data_reads[] = {
      MockRead(first_response),
      MockRead(payload1.c_str()),
      MockRead(net::SYNCHRONOUS, net::OK),
    };
    std::string origin = has_origin ? "Origin: foo.com\r\n" : "";

    std::string m(method);
    std::string trailer =
        (m == "HEAD" || m == "PUT" || m == "POST") ?
            "Content-Length: 0\r\n" : "";

    std::string request1 =
        base::StringPrintf("%s http://www.google.com/ HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n%s%s"
                           "User-Agent:\r\n"
                           "Accept-Encoding: gzip, deflate\r\n\r\n",
                           method, origin.c_str(), trailer.c_str());
    MockWrite data_writes[] = {
      MockWrite(request1.c_str()),
    };
    StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                  data_writes, arraysize(data_writes));
    mock_socket_factory_.AddSocketDataProvider(&data1);

      std::string response2;
      std::string request2;
      if (expected_bad_proxy_count >= 2u ||
          (m != "POST" && expected_retry && expected_bad_proxy_count == 0u)) {
        response2 =
            "HTTP/1.0 200 OK\r\n"
            "Server: not-proxy\r\n\r\n";
        request2 = base::StringPrintf(
            "%s / HTTP/1.1\r\n"
            "Host: www.google.com\r\n"
            "Connection: keep-alive\r\n%s%s"
            "User-Agent:\r\n"
            "Accept-Encoding: gzip, deflate\r\n\r\n",
            method, origin.c_str(), trailer.c_str());
      } else if (expected_bad_proxy_count <= 1u) {
        response2 =
            "HTTP/1.0 200 OK\r\n"
            "Server: not-proxy\r\n"
            "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n";
        request2 = base::StringPrintf(
            "%s http://www.google.com/ HTTP/1.1\r\n"
            "Host: www.google.com\r\n"
            "Proxy-Connection: keep-alive\r\n%s%s"
            "User-Agent:\r\n"
            "Accept-Encoding: gzip, deflate\r\n\r\n",
            method, origin.c_str(), trailer.c_str());
      }
      MockRead data_reads2[] = {
          MockRead(response2.c_str()),
          MockRead("content"),
          MockRead(net::SYNCHRONOUS, net::OK),
      };
      MockWrite data_writes2[] = {
          MockWrite(request2.c_str()),
      };
      StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                     data_writes2, arraysize(data_writes2));
      if (expected_retry) {
        mock_socket_factory_.AddSocketDataProvider(&data2);
    }

    // Expect that we get "content" and not "Bypass message", and that there's
    // a "not-proxy" "Server:" header in the final response.
    ExecuteRequestExpectingContentAndHeader(
        method,
        (expect_response_body ? "content" : ""),
        "server",
        (expected_retry == 0 ? "proxy" : "not-proxy"),
        has_origin,
        expected_retry);
  }

  // Starts a request with the given |method| and checks that the response
  // contains |content| and the the header |header|: |value|, if |header| is
  // non-empty. Verifies that the request's URL chain is the right length
  // depending on whether or not a retry was expected (|expected_retry|).
  void ExecuteRequestExpectingContentAndHeader(const std::string& method,
                                               const std::string& content,
                                               const std::string& header,
                                               const std::string& value,
                                               bool has_origin,
                                               bool expected_retry) {
    TestDelegate d;
    scoped_ptr<URLRequest> r(context_->CreateRequest(
        GURL("http://www.google.com/"),
        net::DEFAULT_PRIORITY,
        &d,
        NULL));
    r->set_method(method);
    r->SetLoadFlags(net::LOAD_NORMAL);
    if (has_origin)
      r->SetExtraRequestHeaderByName("Origin", "foo.com", true);

    r->Start();
    base::RunLoop().Run();

    EXPECT_EQ(net::URLRequestStatus::SUCCESS, r->status().status());
    EXPECT_EQ(net::OK, r->status().error());
    if (expected_retry)
      EXPECT_EQ(2, network_delegate_->headers_received_count());
    else
      EXPECT_EQ(1, network_delegate_->headers_received_count());

    if (!header.empty()) {
      // We also have a server header here that isn't set by the proxy.
      EXPECT_TRUE(r->response_headers()->HasHeaderValue(header, value));
    }

    EXPECT_EQ(content, d.data_received());
  }

  // Returns the key to the |ProxyRetryInfoMap|.
  std::string GetProxyKey(std::string proxy) {
    GURL gurl(proxy);
    std::string host_port = HostPortPair::FromURL(GURL(proxy)).ToString();
    if (gurl.SchemeIs("https"))
      return "https://" + host_port;
    return host_port;
  }

  // Checks that |expected_num_bad_proxies| proxies are on the proxy retry list.
  // If the list has one proxy, it should match |bad_proxy|. If it has two
  // proxies, it should match |bad_proxy| and |bad_proxy2|. Checks also that
  // the current delay associated with each bad proxy is |duration_seconds|.
  void TestBadProxies(unsigned int expected_num_bad_proxies,
                      int duration_seconds,
                      const std::string& bad_proxy,
                      const std::string& bad_proxy2) {
    const ProxyRetryInfoMap& retry_info = proxy_service_->proxy_retry_info();
    ASSERT_EQ(expected_num_bad_proxies, retry_info.size());

    base::TimeDelta expected_min_duration;
    base::TimeDelta expected_max_duration;
    if (duration_seconds == 0) {
      expected_min_duration = base::TimeDelta::FromMinutes(1);
      expected_max_duration = base::TimeDelta::FromMinutes(5);
    } else {
      expected_min_duration = base::TimeDelta::FromSeconds(duration_seconds);
      expected_max_duration = base::TimeDelta::FromSeconds(duration_seconds);
    }

    if (expected_num_bad_proxies >= 1u) {
      ProxyRetryInfoMap::const_iterator i =
          retry_info.find(GetProxyKey(bad_proxy));
      ASSERT_TRUE(i != retry_info.end());
      EXPECT_TRUE(expected_min_duration <= (*i).second.current_delay);
      EXPECT_TRUE((*i).second.current_delay <= expected_max_duration);
    }
    if (expected_num_bad_proxies == 2u) {
      ProxyRetryInfoMap::const_iterator i =
          retry_info.find(GetProxyKey(bad_proxy2));
      ASSERT_TRUE(i != retry_info.end());
      EXPECT_TRUE(expected_min_duration <= (*i).second.current_delay);
      EXPECT_TRUE((*i).second.current_delay <= expected_max_duration);
    }
  }

 protected:
  base::MessageLoopForIO loop_;

  scoped_ptr<net::URLRequestInterceptor> simple_interceptor_;
  net::MockClientSocketFactory mock_socket_factory_;
  scoped_ptr<net::TestNetworkDelegate> network_delegate_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<TestDataReductionProxyParams> proxy_params_;
  scoped_ptr<DataReductionProxyUsageStats> usage_stats_;
  net::StaticHttpUserAgentSettings http_user_agent_settings_;

  scoped_ptr<net::URLRequestInterceptingJobFactory> job_factory_;
  scoped_ptr<TestURLRequestContext> context_;
};

// Tests that request are deemed idempotent or not according to the method used.
TEST_F(DataReductionProxyProtocolTest, TestIdempotency) {
  net::TestURLRequestContext context;
  const struct {
    const char* method;
    bool expected_result;
  } tests[] = {
      { "GET", true },
      { "OPTIONS", true },
      { "HEAD", true },
      { "PUT", true },
      { "DELETE", true },
      { "TRACE", true },
      { "POST", false },
      { "CONNECT", false },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    scoped_ptr<net::URLRequest> request(
        context.CreateRequest(GURL("http://www.google.com/"),
                              net::DEFAULT_PRIORITY,
                              NULL,
                              NULL));
    request->set_method(tests[i].method);
    EXPECT_EQ(tests[i].expected_result, IsRequestIdempotent(request.get()));
  }
}

// After each test, the proxy retry info will contain zero, one, or two of the
// data reduction proxies depending on whether no bypass was indicated by the
// initial response, a single proxy bypass was indicated, or a double bypass
// was indicated. In both the single and double bypass cases, if the request
// was idempotent, it will be retried over a direct connection.
TEST_F(DataReductionProxyProtocolTest, BypassLogic) {
  const struct {
    const char* method;
    const char* first_response;
    bool has_origin;
    bool expected_retry;
    size_t expected_bad_proxy_count;
    bool expect_response_body;
    int expected_duration;
    DataReductionProxyBypassType expected_bypass_type;
  } tests[] = {
    // Valid data reduction proxy response with no bypass message.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      false,
      0u,
      true,
      -1,
      BYPASS_EVENT_TYPE_MAX,
    },
    // Valid data reduction proxy response with older, but still valid via
    // header.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome Compression Proxy\r\n\r\n",
      false,
      false,
      0u,
      true,
      -1,
      BYPASS_EVENT_TYPE_MAX
    },
    // Valid data reduction proxy response with chained via header,
    // no bypass message.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy, 1.0 some-other-proxy\r\n\r\n",
      false,
      false,
      0u,
      true,
      -1,
      BYPASS_EVENT_TYPE_MAX
    },
    // Valid data reduction proxy response with a bypass message.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Valid data reduction proxy response with a bypass message.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=1\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
    // Same as above with the OPTIONS method, which is idempotent.
    { "OPTIONS",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Same as above with the HEAD method, which is idempotent.
    { "HEAD",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      false,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Same as above with the PUT method, which is idempotent.
    { "PUT",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Same as above with the DELETE method, which is idempotent.
    { "DELETE",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Same as above with the TRACE method, which is idempotent.
    { "TRACE",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // 500 responses should be bypassed.
    { "GET",
      "HTTP/1.1 500 Internal Server Error\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR
    },
    // 502 responses should be bypassed.
    { "GET",
      "HTTP/1.1 502 Internal Server Error\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY
    },
    // 503 responses should be bypassed.
    { "GET",
      "HTTP/1.1 503 Internal Server Error\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE
    },
    // Invalid data reduction proxy 4xx response. Missing Via header.
    { "GET",
      "HTTP/1.1 404 Not Found\r\n"
      "Server: proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      1,
      BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_4XX
    },
    // Invalid data reduction proxy response. Missing Via header.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER
    },
    // Invalid data reduction proxy response. Wrong Via header.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Via: 1.0 some-other-proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER
    },
    // Valid data reduction proxy response. 304 missing Via header.
    { "GET",
      "HTTP/1.1 304 Not Modified\r\n"
      "Server: proxy\r\n\r\n",
      false,
      false,
      0u,
      false,
      0,
      BYPASS_EVENT_TYPE_MAX
    },
    // Valid data reduction proxy response with a bypass message. It will
    // not be retried because the request is non-idempotent.
    { "POST",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      false,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Valid data reduction proxy response with block message. Both proxies
    // should be on the retry list when it completes.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block=1\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      2u,
      true,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
    // Valid data reduction proxy response with a block-once message. It will be
    // retried, and there will be no proxies on the retry list since block-once
    // only affects the current request.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      0u,
      true,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the OPTIONS method, which is idempotent.
    { "OPTIONS",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      0u,
      true,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the HEAD method, which is idempotent.
    { "HEAD",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      0u,
      false,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the PUT method, which is idempotent.
    { "PUT",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      0u,
      true,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the DELETE method, which is idempotent.
    { "DELETE",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      0u,
      true,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the TRACE method, which is idempotent.
    { "TRACE",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      0u,
      true,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Valid data reduction proxy response with a block-once message. It will
    // not be retried because the request is non-idempotent, and there will be
    // no proxies on the retry list since block-once only affects the current
    // request.
    { "POST",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      false,
      0u,
      true,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Valid data reduction proxy response with block and block-once messages.
    // The block message will override the block-once message, so both proxies
    // should be on the retry list when it completes.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block=1, block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      2u,
      true,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
    // Valid data reduction proxy response with bypass and block-once messages.
    // The bypass message will override the block-once message, so one proxy
    // should be on the retry list when it completes.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=1, block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      true,
      1u,
      true,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
  };
  std::string primary = proxy_params_->DefaultOrigin();
  std::string fallback = proxy_params_->DefaultFallbackOrigin();
  for (size_t i = 0; i < arraysize(tests); ++i) {
    DataReductionProxyBypassType bypass_type;
    ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
        "PROXY " +
        HostPortPair::FromURL(GURL(primary)).ToString() + "; PROXY " +
        HostPortPair::FromURL(GURL(fallback)).ToString() + "; DIRECT"),
        &bypass_type);
    TestProxyFallback(tests[i].method,
                      tests[i].first_response,
                      tests[i].has_origin,
                      tests[i].expected_retry,
                      tests[i].expected_bad_proxy_count,
                      tests[i].expect_response_body);
    EXPECT_EQ(tests[i].expected_bypass_type, usage_stats_->GetBypassType());
    // We should also observe the bad proxy in the retry list.
    TestBadProxies(tests[i].expected_bad_proxy_count,
                   tests[i].expected_duration,
                   primary, fallback);
  }
}

TEST_F(DataReductionProxyProtocolTest,
       ProxyBypassIgnoredOnDirectConnection) {
  // Verify that a Chrome-Proxy header is ignored when returned from a directly
  // connected origin server.
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult("DIRECT"),
                            NULL);

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Chrome-Proxy: bypass=0\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(net::SYNCHRONOUS, net::OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent:\r\n"
              "Accept-Encoding: gzip, deflate\r\n\r\n"),
  };
  StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                 data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data1);

  TestDelegate d;
  scoped_ptr<URLRequest> r(context_->CreateRequest(
      GURL("http://www.google.com/"),
      net::DEFAULT_PRIORITY,
      &d,
      NULL));
  r->set_method("GET");
  r->SetLoadFlags(net::LOAD_NORMAL);

  r->Start();
  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, r->status().status());
  EXPECT_EQ(net::OK, r->status().error());

  EXPECT_EQ("Bypass message", d.data_received());

  // We should have no entries in our bad proxy list.
  TestBadProxies(0, -1, "", "");
}

class BadEntropyProvider : public base::FieldTrial::EntropyProvider {
 public:
  ~BadEntropyProvider() override {}

  double GetEntropyForTrial(const std::string& trial_name,
                            uint32 randomization_seed) const override {
    return 0.5;
  }
};

TEST_F(DataReductionProxyProtocolTest, OnResolveProxyHandler) {
  int load_flags = net::LOAD_NORMAL;
  GURL url("http://www.google.com/");
  net::ProxyConfig proxy_config_direct = net::ProxyConfig::CreateDirect();

  TestDataReductionProxyParams test_params(
            DataReductionProxyParams::kAllowed |
            DataReductionProxyParams::kFallbackAllowed |
            DataReductionProxyParams::kPromoAllowed,
            TestDataReductionProxyParams::HAS_EVERYTHING &
            ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
            ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN);

  // Data reduction proxy info
  net::ProxyInfo data_reduction_proxy_info;
  std::string data_reduction_proxy;
  base::TrimString(test_params.DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);
  EXPECT_FALSE(data_reduction_proxy_info.is_empty());

  // Data reduction proxy config
  net::ProxyConfig data_reduction_proxy_config;
  data_reduction_proxy_config.proxy_rules().ParseFromString(
      "http=" + data_reduction_proxy + ",direct://;");
  data_reduction_proxy_config.set_id(1);

  // Other proxy info
  net::ProxyInfo other_proxy_info;
  other_proxy_info.UseNamedProxy("proxy.com");
  EXPECT_FALSE(other_proxy_info.is_empty());

  // Direct
   net::ProxyInfo direct_proxy_info;
   direct_proxy_info.UseDirect();
   EXPECT_TRUE(direct_proxy_info.is_direct());

   // Empty retry info map
   net::ProxyRetryInfoMap empty_proxy_retry_info;

   // Retry info map with the data reduction proxy;
   net::ProxyRetryInfoMap data_reduction_proxy_retry_info;
   net::ProxyRetryInfo retry_info;
   retry_info.current_delay = base::TimeDelta::FromSeconds(1000);
   retry_info.bad_until = base::TimeTicks().Now() + retry_info.current_delay;
   retry_info.try_while_bad = false;
   data_reduction_proxy_retry_info[
       data_reduction_proxy_info.proxy_server().ToURI()] = retry_info;

   net::ProxyInfo result;

   // The data reduction proxy is used. It should be used afterwards.
   result.Use(data_reduction_proxy_info);
   OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                         proxy_config_direct, empty_proxy_retry_info,
                         &test_params, &result);
   EXPECT_EQ(data_reduction_proxy_info.proxy_server(), result.proxy_server());

   // Another proxy is used. It should be used afterwards.
   result.Use(other_proxy_info);
   OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                         proxy_config_direct, empty_proxy_retry_info,
                         &test_params, &result);
   EXPECT_EQ(other_proxy_info.proxy_server(), result.proxy_server());

   // A direct connection is used. The data reduction proxy should be used
   // afterwards.
   // Another proxy is used. It should be used afterwards.
   result.Use(direct_proxy_info);
   net::ProxyConfig::ID prev_id = result.config_id();
   OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                         proxy_config_direct, empty_proxy_retry_info,
                         &test_params, &result);
   EXPECT_EQ(data_reduction_proxy_info.proxy_server(), result.proxy_server());
   // Only the proxy list should be updated, not he proxy info.
   EXPECT_EQ(result.config_id(), prev_id);

   // A direct connection is used, but the data reduction proxy is on the retry
   // list. A direct connection should be used afterwards.
   result.Use(direct_proxy_info);
   prev_id = result.config_id();
   OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                         proxy_config_direct, data_reduction_proxy_retry_info,
                         &test_params, &result);
   EXPECT_TRUE(result.proxy_server().is_direct());
   EXPECT_EQ(result.config_id(), prev_id);


  // Without DataCompressionProxyCriticalBypass Finch trial set, should never
  // bypass.
  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        proxy_config_direct, empty_proxy_retry_info,
                        &test_params, &data_reduction_proxy_info);
  EXPECT_FALSE(data_reduction_proxy_info.is_direct());

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        proxy_config_direct, empty_proxy_retry_info,
                        &test_params, &other_proxy_info);
  EXPECT_FALSE(other_proxy_info.is_direct());

  load_flags |= net::LOAD_BYPASS_DATA_REDUCTION_PROXY;

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        proxy_config_direct, empty_proxy_retry_info,
                        &test_params, &data_reduction_proxy_info);
  EXPECT_FALSE(data_reduction_proxy_info.is_direct());

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        proxy_config_direct, empty_proxy_retry_info,
                        &test_params, &other_proxy_info);
  EXPECT_FALSE(other_proxy_info.is_direct());

  // With Finch trial set, should only bypass if LOAD flag is set and the
  // effective proxy is the data compression proxy.
  base::FieldTrialList field_trial_list(new BadEntropyProvider());
  base::FieldTrialList::CreateFieldTrial("DataCompressionProxyCriticalBypass",
                                         "Enabled");
  EXPECT_TRUE(
      DataReductionProxyParams::IsIncludedInCriticalPathBypassFieldTrial());

  load_flags = net::LOAD_NORMAL;

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        proxy_config_direct, empty_proxy_retry_info,
                        &test_params, &data_reduction_proxy_info);
  EXPECT_FALSE(data_reduction_proxy_info.is_direct());

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        proxy_config_direct, empty_proxy_retry_info,
                        &test_params, &other_proxy_info);
  EXPECT_FALSE(other_proxy_info.is_direct());

  load_flags |= net::LOAD_BYPASS_DATA_REDUCTION_PROXY;

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        proxy_config_direct, empty_proxy_retry_info,
                        &test_params, &other_proxy_info);
  EXPECT_FALSE(other_proxy_info.is_direct());

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        proxy_config_direct, empty_proxy_retry_info,
                        &test_params, &data_reduction_proxy_info);
  EXPECT_TRUE(data_reduction_proxy_info.is_direct());
}

}  // namespace data_reduction_proxy
