// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"

#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Transform "normal"-looking headers (\n-separated) to the appropriate
// input format for ParseRawHeaders (\0-separated).
void HeadersToRaw(std::string* headers) {
  std::replace(headers->begin(), headers->end(), '\n', '\0');
  if (!headers->empty())
    *headers += '\0';
}

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyHeadersTest : public testing::Test {};

TEST_F(DataReductionProxyHeadersTest, GetProxyBypassInfo) {
  const struct {
     const char* headers;
     bool expected_result;
     int64 expected_retry_delay;
     bool expected_bypass_all;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=-1\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=xyz\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: foo=abc, bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=86400, bar=abc\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=3600\n"
      "Chrome-Proxy: bypass=86400\n"
      "Content-Length: 999\n",
      true,
      3600,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=3600, bypass=86400\n"
      "Content-Length: 999\n",
      true,
      3600,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=, bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass\n"
      "Chrome-Proxy: bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block=, block=3600\n"
      "Content-Length: 999\n",
      true,
      3600,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=86400, block=3600\n"
      "Content-Length: 999\n",
      true,
      3600,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: proxy-bypass\n"
      "Chrome-Proxy: block=, bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: proxy-bypass\n"
      "Chrome-Proxy: block=-1\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: proxy-bypass\n"
      "Chrome-Proxy: block=99999999999999999999\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    DataReductionProxyInfo data_reduction_proxy_info;
    EXPECT_EQ(tests[i].expected_result,
              ParseHeadersAndSetProxyInfo(parsed, &data_reduction_proxy_info));
    EXPECT_EQ(tests[i].expected_retry_delay,
              data_reduction_proxy_info.bypass_duration.InSeconds());
    EXPECT_EQ(tests[i].expected_bypass_all,
              data_reduction_proxy_info.bypass_all);
  }
}

TEST_F(DataReductionProxyHeadersTest, ParseHeadersAndSetProxyInfo) {
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=0\n"
      "Content-Length: 999\n";
  HeadersToRaw(&headers);
  scoped_refptr<net::HttpResponseHeaders> parsed(
      new net::HttpResponseHeaders(headers));

  DataReductionProxyInfo data_reduction_proxy_info;
  EXPECT_TRUE(ParseHeadersAndSetProxyInfo(parsed, &data_reduction_proxy_info));
  EXPECT_LE(60, data_reduction_proxy_info.bypass_duration.InSeconds());
  EXPECT_GE(5 * 60, data_reduction_proxy_info.bypass_duration.InSeconds());
  EXPECT_FALSE(data_reduction_proxy_info.bypass_all);
}

TEST_F(DataReductionProxyHeadersTest, HasDataReductionProxyViaHeader) {
  const struct {
     const char* headers;
     bool expected_result;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Proxy\n",
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1\n",
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.0 Chrome-Compression-Proxy\n",
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Foo-Bar, 1.1 Chrome-Compression-Proxy\n",
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy, 1.1 Bar-Foo\n",
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 chrome-compression-proxy\n",
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Foo-Bar\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Proxy\n",
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome Compression Proxy\n",
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Foo-Bar, 1.1 Chrome Compression Proxy\n",
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome Compression Proxy, 1.1 Bar-Foo\n",
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 chrome compression proxy\n",
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Foo-Bar\n"
      "Via: 1.1 Chrome Compression Proxy\n",
      true,
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    EXPECT_EQ(tests[i].expected_result,
              HasDataReductionProxyViaHeader(parsed));
  }
}

TEST_F(DataReductionProxyHeadersTest, GetDataReductionProxyBypassEventType) {
  const struct {
     const char* headers;
     net::ProxyService::DataReductionProxyBypassType expected_result;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: bypass=0\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::MEDIUM_BYPASS,
    },
    { "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: bypass=1\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::SHORT_BYPASS,
    },
    { "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: bypass=59\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::SHORT_BYPASS,
    },
    { "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: bypass=60\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::MEDIUM_BYPASS,
    },
    { "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: bypass=300\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::MEDIUM_BYPASS,
    },
    { "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: bypass=301\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::LONG_BYPASS,
    },
    { "HTTP/1.1 500 Internal Server Error\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::STATUS_500_HTTP_INTERNAL_SERVER_ERROR,
    },
    { "HTTP/1.1 501 Not Implemented\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::BYPASS_EVENT_TYPE_MAX,
    },
    { "HTTP/1.1 502 Bad Gateway\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::STATUS_502_HTTP_BAD_GATEWAY,
    },
    { "HTTP/1.1 503 Service Unavailable\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::STATUS_503_HTTP_SERVICE_UNAVAILABLE,
    },
    { "HTTP/1.1 504 Gateway Timeout\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::BYPASS_EVENT_TYPE_MAX,
    },
    { "HTTP/1.1 505 HTTP Version Not Supported\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::BYPASS_EVENT_TYPE_MAX,
    },
    { "HTTP/1.1 304 Not Modified\n",
      net::ProxyService::BYPASS_EVENT_TYPE_MAX,
    },
    { "HTTP/1.1 200 OK\n",
      net::ProxyService::MISSING_VIA_HEADER_OTHER,
    },
    { "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: bypass=59\n",
      net::ProxyService::SHORT_BYPASS,
    },
    { "HTTP/1.1 502 Bad Gateway\n",
      net::ProxyService::STATUS_502_HTTP_BAD_GATEWAY,
    },
    { "HTTP/1.1 502 Bad Gateway\n"
      "Chrome-Proxy: bypass=59\n",
      net::ProxyService::SHORT_BYPASS,
    },
    { "HTTP/1.1 502 Bad Gateway\n"
      "Chrome-Proxy: bypass=59\n",
      net::ProxyService::SHORT_BYPASS,
    },
    { "HTTP/1.1 414 Request-URI Too Long\n",
      net::ProxyService::MISSING_VIA_HEADER_4XX,
    },
    { "HTTP/1.1 414 Request-URI Too Long\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::BYPASS_EVENT_TYPE_MAX,
    },
    { "HTTP/1.1 407 Proxy Authentication Required\n",
      net::ProxyService::MALFORMED_407,
    },
    { "HTTP/1.1 407 Proxy Authentication Required\n"
      "Proxy-Authenticate: Basic\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      net::ProxyService::BYPASS_EVENT_TYPE_MAX,
    }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
    DataReductionProxyInfo chrome_proxy_info;
    EXPECT_EQ(tests[i].expected_result,
              GetDataReductionProxyBypassType(parsed, &chrome_proxy_info));
  }
}
}  // namespace data_reduction_proxy
