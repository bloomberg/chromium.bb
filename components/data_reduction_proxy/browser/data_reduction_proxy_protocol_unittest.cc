// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_protocol.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"
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
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

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


namespace {
// Transform "normal"-looking headers (\n-separated) to the appropriate
// input format for ParseRawHeaders (\0-separated).
void HeadersToRaw(std::string* headers) {
  std::replace(headers->begin(), headers->end(), '\n', '\0');
  if (!headers->empty())
    *headers += '\0';
}

} // namespace


namespace data_reduction_proxy {

// A test network delegate that exercises the bypass logic of the data
// reduction proxy.
class TestDataReductionProxyNetworkDelegate : public net::NetworkDelegate {
 public:
  TestDataReductionProxyNetworkDelegate(
      TestDataReductionProxyParams* test_params,
      DataReductionProxyBypassType* bypass_type)
      : net::NetworkDelegate(),
        test_data_reduction_proxy_params_(test_params),
        bypass_type_(bypass_type) {
  }

  virtual int OnHeadersReceived(
      URLRequest* request,
      const net::CompletionCallback& callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE {
    data_reduction_proxy::MaybeBypassProxyAndPrepareToRetry(
        test_data_reduction_proxy_params_,
        request,
        original_response_headers,
        override_response_headers,
        bypass_type_);
    return net::OK;
  }

  TestDataReductionProxyParams* test_data_reduction_proxy_params_;
  DataReductionProxyBypassType* bypass_type_;
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
            ~TestDataReductionProxyParams::HAS_DEV_ORIGIN));
  }

  // Sets up the |TestURLRequestContext| with the provided |ProxyService| and
  // |bypass_type| to store bypass reasons.
  void ConfigureTestDependencies(ProxyService* proxy_service,
                                 DataReductionProxyBypassType* bypass_type) {
    // Create a context with delayed initialization.
    context_.reset(new TestURLRequestContext(true));

    proxy_service_.reset(proxy_service);
    network_delegate_.reset(new TestDataReductionProxyNetworkDelegate(
        proxy_params_.get(), bypass_type));

    context_->set_client_socket_factory(&mock_socket_factory_);
    context_->set_proxy_service(proxy_service_.get());
    context_->set_network_delegate(network_delegate_.get());
    // This is needed to prevent the test context from adding language headers
    // to requests.
    context_->set_http_user_agent_settings(&http_user_agent_settings_);

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
                         bool expected_retry,
                         bool expect_response_body) {
    std::string payload1 =
        (expected_retry ? "Bypass message" : "content");
    MockRead data_reads[] = {
      MockRead(first_response),
      MockRead(payload1.c_str()),
      MockRead(net::SYNCHRONOUS, net::OK),
    };
    std::string m(method);
    std::string trailer =
        (m == "HEAD" || m == "PUT" || m == "POST") ?
            "Content-Length: 0\r\n" : "";

    std::string request1 =
        base::StringPrintf("%s http://www.google.com/ HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n%s"
                           "User-Agent:\r\n"
                           "Accept-Encoding: gzip,deflate\r\n\r\n",
                           method, trailer.c_str());
    MockWrite data_writes[] = {
      MockWrite(request1.c_str()),
    };
    StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                  data_writes, arraysize(data_writes));
    mock_socket_factory_.AddSocketDataProvider(&data1);

      MockRead data_reads2[] = {
        MockRead("HTTP/1.0 200 OK\r\n"
                 "Server: not-proxy\r\n\r\n"),
        MockRead("content"),
        MockRead(net::SYNCHRONOUS, net::OK),
      };
      std::string request2 =
          base::StringPrintf("%s / HTTP/1.1\r\n"
                             "Host: www.google.com\r\n"
                             "Connection: keep-alive\r\n%s"
                             "User-Agent:\r\n"
                             "Accept-Encoding: gzip,deflate\r\n\r\n",
                             method, trailer.c_str());
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
                                               bool expected_retry) {
    TestDelegate d;
    URLRequest r(GURL("http://www.google.com/"),
                 net::DEFAULT_PRIORITY,
                 &d,
                 context_.get());
    r.set_method(method);
    r.SetLoadFlags(net::LOAD_NORMAL);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(net::URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(net::OK, r.status().error());
    if (expected_retry)
      EXPECT_EQ(2U, r.url_chain().size());
    else
      EXPECT_EQ(1U, r.url_chain().size());

    if (!header.empty()) {
      // We also have a server header here that isn't set by the proxy.
      EXPECT_TRUE(r.response_headers()->HasHeaderValue(header, value));
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
    }
    else {
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

  net::MockClientSocketFactory mock_socket_factory_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<TestDataReductionProxyParams> proxy_params_;
  scoped_ptr<TestDataReductionProxyNetworkDelegate> network_delegate_;
  net::StaticHttpUserAgentSettings http_user_agent_settings_;

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
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    net::TestURLRequest request(GURL("http://www.google.com/"),
                                net::DEFAULT_PRIORITY,
                                NULL,
                                &context);
    request.set_method(tests[i].method);
    EXPECT_EQ(tests[i].expected_result, IsRequestIdempotent(&request));
  }
}

// Tests that the response is correctly overwritten as a redirect.
TEST_F(DataReductionProxyProtocolTest, OverrideResponseAsRedirect) {
  net::TestURLRequestContext context;
  const struct {
    const char* headers;
    const char* expected_headers;
  } tests[] = {
      { "HTTP/1.1 200 0K\n"
        "Chrome-Proxy: block=1\n"
        "Via: 1.1 Chrome-Compression-Proxy\n",

        "HTTP/1.1 302 Found\n"
        "Chrome-Proxy: block=1\n"
        "Via: 1.1 Chrome-Compression-Proxy\n"
        "Location: http://www.google.com/\n"
      },
      { "HTTP/1.1 200 0K\n"
        "Chrome-Proxy: block-once\n"
        "Via: 1.1 Chrome-Compression-Proxy\n",

        "HTTP/1.1 302 Found\n"
        "Chrome-Proxy: block-once\n"
        "Via: 1.1 Chrome-Compression-Proxy\n"
        "Location: http://www.google.com/\n"
      },
      { "HTTP/1.1 302 Found\n"
        "Location: http://foo.com/\n",

        "HTTP/1.1 302 Found\n"
        "Location: http://www.google.com/\n"
      },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<HttpResponseHeaders> original_response_headers(
        new HttpResponseHeaders(headers));
    scoped_refptr<HttpResponseHeaders> override_response_headers;
    TestDelegate test_delegate;
    net::TestURLRequest request(GURL("http://www.google.com/"),
                           net::DEFAULT_PRIORITY,
                           NULL,
                           &context);
    OverrideResponseAsRedirect(&request,
                               original_response_headers,
                               &override_response_headers);
    int expected_flags = net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_PROXY;
    EXPECT_EQ(expected_flags, request.load_flags());
    std::string override_headers;
    override_response_headers->GetNormalizedHeaders(&override_headers);
    EXPECT_EQ(std::string(tests[i].expected_headers), override_headers);
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
      true,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE
    },
    // Invalid data reduction proxy response. Missing Via header.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n\r\n",
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
      true,
      1u,
      true,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
  };
  std::string primary = proxy_params_->DefaultOrigin();
  std::string fallback = proxy_params_->DefaultFallbackOrigin();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    DataReductionProxyBypassType bypass_type;
    ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
        "PROXY " +
        HostPortPair::FromURL(GURL(primary)).ToString() + "; PROXY " +
        HostPortPair::FromURL(GURL(fallback)).ToString() + "; DIRECT"),
        &bypass_type);
    TestProxyFallback(tests[i].method,
                      tests[i].first_response,
                      tests[i].expected_retry,
                      tests[i].expect_response_body);

    EXPECT_EQ(tests[i].expected_bypass_type, bypass_type);

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
              "Accept-Encoding: gzip,deflate\r\n\r\n"),
  };
  StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                 data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data1);

  TestDelegate d;
  URLRequest r(GURL("http://www.google.com/"),
               net::DEFAULT_PRIORITY,
               &d,
               context_.get());
   r.set_method("GET");
   r.SetLoadFlags(net::LOAD_NORMAL);

   r.Start();
   base::RunLoop().Run();

   EXPECT_EQ(net::URLRequestStatus::SUCCESS, r.status().status());
   EXPECT_EQ(net::OK, r.status().error());

  EXPECT_EQ("Bypass message", d.data_received());

  // We should have no entries in our bad proxy list.
  TestBadProxies(0, -1, "", "");
}

class BadEntropyProvider : public base::FieldTrial::EntropyProvider {
 public:
  virtual ~BadEntropyProvider() {}

  virtual double GetEntropyForTrial(const std::string& trial_name,
                                    uint32 randomization_seed) const OVERRIDE {
    return 0.5;
  }
};

TEST_F(DataReductionProxyProtocolTest, OnResolveProxyHandler) {
  int load_flags = net::LOAD_NORMAL;
  GURL url("http://www.google.com/");

  TestDataReductionProxyParams test_params(
            DataReductionProxyParams::kAllowed |
            DataReductionProxyParams::kFallbackAllowed |
            DataReductionProxyParams::kPromoAllowed,
            TestDataReductionProxyParams::HAS_EVERYTHING &
            ~TestDataReductionProxyParams::HAS_DEV_ORIGIN);

  // Data reduction proxy
  net::ProxyInfo info1;
  std::string data_reduction_proxy;
  base::TrimString(test_params.DefaultOrigin(), "/", &data_reduction_proxy);
  info1.UseNamedProxy(data_reduction_proxy);
  EXPECT_FALSE(info1.is_empty());

  // Other proxy
  net::ProxyInfo info2;
  info2.UseNamedProxy("proxy.com");
  EXPECT_FALSE(info2.is_empty());

  // Without DataCompressionProxyCriticalBypass Finch trial set, should never
  // bypass.
  OnResolveProxyHandler(url, load_flags, &test_params, &info1);
  EXPECT_FALSE(info1.is_direct());

  OnResolveProxyHandler(url, load_flags, &test_params,&info2);
  EXPECT_FALSE(info2.is_direct());

  load_flags |= net::LOAD_BYPASS_DATA_REDUCTION_PROXY;

  OnResolveProxyHandler(url, load_flags, &test_params, &info1);
  EXPECT_FALSE(info1.is_direct());

  OnResolveProxyHandler(url, load_flags, &test_params, &info2);
  EXPECT_FALSE(info2.is_direct());

  // With Finch trial set, should only bypass if LOAD flag is set and the
  // effective proxy is the data compression proxy.
  base::FieldTrialList field_trial_list(new BadEntropyProvider());
  base::FieldTrialList::CreateFieldTrial("DataCompressionProxyRollout",
                                         "Enabled");
  base::FieldTrialList::CreateFieldTrial("DataCompressionProxyCriticalBypass",
                                         "Enabled");
  EXPECT_TRUE(
      DataReductionProxyParams::IsIncludedInCriticalPathBypassFieldTrial());

  load_flags = net::LOAD_NORMAL;

  OnResolveProxyHandler(url, load_flags, &test_params, &info1);
  EXPECT_FALSE(info1.is_direct());

  OnResolveProxyHandler(url, load_flags, &test_params, &info2);
  EXPECT_FALSE(info2.is_direct());

  load_flags |= net::LOAD_BYPASS_DATA_REDUCTION_PROXY;

  OnResolveProxyHandler(url, load_flags, &test_params, &info2);
  EXPECT_FALSE(info2.is_direct());

  OnResolveProxyHandler(url, load_flags, &test_params, &info1);
  EXPECT_TRUE(info1.is_direct());
}

}  // namespace data_reduction_proxy
