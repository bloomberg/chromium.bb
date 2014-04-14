// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_log.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_transaction_unittest.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

class HttpNetworkLayerTest : public PlatformTest {
 protected:
  HttpNetworkLayerTest() : ssl_config_service_(new SSLConfigServiceDefaults) {}

  virtual void SetUp() {
    ConfigureTestDependencies(ProxyService::CreateDirect());
  }

  void ConfigureTestDependencies(ProxyService* proxy_service) {
    cert_verifier_.reset(new MockCertVerifier);
    transport_security_state_.reset(new TransportSecurityState);
    proxy_service_.reset(proxy_service);
    HttpNetworkSession::Params session_params;
    session_params.client_socket_factory = &mock_socket_factory_;
    session_params.host_resolver = &host_resolver_;
    session_params.cert_verifier = cert_verifier_.get();
    session_params.transport_security_state = transport_security_state_.get();
    session_params.proxy_service = proxy_service_.get();
    session_params.ssl_config_service = ssl_config_service_.get();
    session_params.http_server_properties =
        http_server_properties_.GetWeakPtr();
    network_session_ = new HttpNetworkSession(session_params);
    factory_.reset(new HttpNetworkLayer(network_session_.get()));
  }

#if defined(SPDY_PROXY_AUTH_ORIGIN)
  std::string GetDataReductionProxy() {
    return HostPortPair::FromURL(GURL(SPDY_PROXY_AUTH_ORIGIN)).ToString();
  }
#endif

#if defined(SPDY_PROXY_AUTH_ORIGIN) && defined(DATA_REDUCTION_FALLBACK_HOST)
  std::string GetChromeFallbackProxy() {
    return HostPortPair::FromURL(GURL(DATA_REDUCTION_FALLBACK_HOST)).ToString();
  }
#endif

  void ExecuteRequestExpectingContentAndHeader(const std::string& method,
                                               const std::string& content,
                                               const std::string& header,
                                               const std::string& value) {
    TestCompletionCallback callback;

    HttpRequestInfo request_info;
    request_info.url = GURL("http://www.google.com/");
    request_info.method = method;
    request_info.load_flags = LOAD_NORMAL;

    scoped_ptr<HttpTransaction> trans;
    int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
    EXPECT_EQ(OK, rv);

    rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(OK, rv);

    std::string contents;
    rv = ReadTransaction(trans.get(), &contents);
    EXPECT_EQ(OK, rv);
    EXPECT_EQ(content, contents);

    if (!header.empty()) {
      // We also have a server header here that isn't set by the proxy.
      EXPECT_TRUE(trans->GetResponseInfo()->headers->HasHeaderValue(
          header, value));
    }
  }

  // Check that |proxy_count| proxies are in the retry list.
  // These will be, in order, |bad_proxy| and |bad_proxy2|".
  void TestBadProxies(unsigned int proxy_count, const std::string& bad_proxy,
                      const std::string& bad_proxy2) {
    const ProxyRetryInfoMap& retry_info = proxy_service_->proxy_retry_info();
    ASSERT_EQ(proxy_count, retry_info.size());
    if (proxy_count > 0)
      ASSERT_TRUE(retry_info.find(bad_proxy) != retry_info.end());
    if (proxy_count > 1)
      ASSERT_TRUE(retry_info.find(bad_proxy2) != retry_info.end());
  }

  // Simulates a request through a proxy which returns a bypass, which is then
  // retried through a second proxy that doesn't bypass.
  // Checks that the expected requests were issued, the expected content was
  // recieved, and the first proxy |bad_proxy| was marked as bad.
  void TestProxyFallback(const std::string& bad_proxy) {
    MockRead data_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Chrome-Proxy: bypass=0\r\n\r\n"),
      MockRead("Bypass message"),
      MockRead(SYNCHRONOUS, OK),
    };
    TestProxyFallbackWithMockReads(bad_proxy, "", data_reads,
                                   arraysize(data_reads), 1u);
  }

  void TestProxyFallbackWithMockReads(const std::string& bad_proxy,
                                      const std::string& bad_proxy2,
                                      MockRead data_reads[],
                                      int data_reads_size,
                                      unsigned int expected_retry_info_size) {
    TestProxyFallbackByMethodWithMockReads(bad_proxy, bad_proxy2, data_reads,
                                           data_reads_size, "GET", "content",
                                           true, expected_retry_info_size);
  }

  void TestProxyFallbackByMethodWithMockReads(
      const std::string& bad_proxy,
      const std::string& bad_proxy2,
      MockRead data_reads[],
      int data_reads_size,
      std::string method,
      std::string content,
      bool retry_expected,
      unsigned int expected_retry_info_size) {
    std::string trailer =
        (method == "HEAD" || method == "PUT" || method == "POST") ?
        "Content-Length: 0\r\n\r\n" : "\r\n";
    std::string request =
        base::StringPrintf("%s http://www.google.com/ HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n"
                           "%s", method.c_str(), trailer.c_str());

    MockWrite data_writes[] = {
      MockWrite(request.c_str()),
    };

    StaticSocketDataProvider data1(data_reads, data_reads_size,
                                  data_writes, arraysize(data_writes));
    mock_socket_factory_.AddSocketDataProvider(&data1);

    // Second data provider returns the expected content.
    MockRead data_reads2[3];
    size_t data_reads2_index = 0;
    data_reads2[data_reads2_index++] = MockRead("HTTP/1.0 200 OK\r\n"
                                                "Server: not-proxy\r\n\r\n");
    if (!content.empty())
      data_reads2[data_reads2_index++] = MockRead(content.c_str());
    data_reads2[data_reads2_index++] = MockRead(SYNCHRONOUS, OK);

    MockWrite data_writes2[] = {
      MockWrite(request.c_str()),
    };
    StaticSocketDataProvider data2(data_reads2, data_reads2_index,
                                  data_writes2, arraysize(data_writes2));
    mock_socket_factory_.AddSocketDataProvider(&data2);

    // Expect that we get "content" and not "Bypass message", and that there's
    // a "not-proxy" "Server:" header in the final response.
    if (retry_expected) {
      ExecuteRequestExpectingContentAndHeader(method, content,
                                              "server", "not-proxy");
    } else {
      ExecuteRequestExpectingContentAndHeader(method, content, "", "");
    }

    // We should also observe the bad proxy in the retry list.
    TestBadProxies(expected_retry_info_size, bad_proxy, bad_proxy2);
  }

  // Simulates a request through a proxy which returns a bypass, which is then
  // retried through a direct connection to the origin site.
  // Checks that the expected requests were issued, the expected content was
  // received, and the proxy |bad_proxy| was marked as bad.
  void TestProxyFallbackToDirect(const std::string& bad_proxy) {
    MockRead data_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Chrome-Proxy: bypass=0\r\n\r\n"),
      MockRead("Bypass message"),
      MockRead(SYNCHRONOUS, OK),
    };
    MockWrite data_writes[] = {
      MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
    };
    StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                  data_writes, arraysize(data_writes));
    mock_socket_factory_.AddSocketDataProvider(&data1);

    // Second data provider returns the expected content.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"
               "Server: not-proxy\r\n\r\n"),
      MockRead("content"),
      MockRead(SYNCHRONOUS, OK),
    };
    MockWrite data_writes2[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n\r\n"),
    };
    StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                   data_writes2, arraysize(data_writes2));
    mock_socket_factory_.AddSocketDataProvider(&data2);

    // Expect that we get "content" and not "Bypass message", and that there's
    // a "not-proxy" "Server:" header in the final response.
    ExecuteRequestExpectingContentAndHeader("GET", "content",
                                            "server", "not-proxy");

    // We should also observe the bad proxy in the retry list.
    TestBadProxies(1u, bad_proxy, "");
  }

  // Simulates a request through a proxy which returns a bypass, under a
  // configuration where there is no valid bypass. |proxy_count| proxies
  // are expected to be configured.
  // Checks that the expected requests were issued, the bypass message was the
  // final received content,  and all proxies were marked as bad.
  void TestProxyFallbackFail(unsigned int proxy_count,
                             const std::string& bad_proxy,
                             const std::string& bad_proxy2) {
    MockRead data_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Chrome-Proxy: bypass=0\r\n\r\n"),
      MockRead("Bypass message"),
      MockRead(SYNCHRONOUS, OK),
    };
    MockWrite data_writes[] = {
      MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
    };
    StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                   data_writes, arraysize(data_writes));
    StaticSocketDataProvider data2(data_reads, arraysize(data_reads),
                                   data_writes, arraysize(data_writes));

    mock_socket_factory_.AddSocketDataProvider(&data1);
    if (proxy_count > 1)
      mock_socket_factory_.AddSocketDataProvider(&data2);

    // Expect that we get "Bypass message", and not "content"..
    ExecuteRequestExpectingContentAndHeader("GET", "Bypass message", "", "");

    // We should also observe the bad proxy or proxies in the retry list.
    TestBadProxies(proxy_count, bad_proxy, bad_proxy2);
  }

  MockClientSocketFactory mock_socket_factory_;
  MockHostResolver host_resolver_;
  scoped_ptr<CertVerifier> cert_verifier_;
  scoped_ptr<TransportSecurityState> transport_security_state_;
  scoped_ptr<ProxyService> proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;
  scoped_refptr<HttpNetworkSession> network_session_;
  scoped_ptr<HttpNetworkLayer> factory_;
  HttpServerPropertiesImpl http_server_properties_;
};

TEST_F(HttpNetworkLayerTest, CreateAndDestroy) {
  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(trans.get() != NULL);
}

TEST_F(HttpNetworkLayerTest, Suspend) {
  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(OK, rv);

  trans.reset();

  factory_->OnSuspend();

  rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(ERR_NETWORK_IO_SUSPENDED, rv);

  ASSERT_TRUE(trans == NULL);

  factory_->OnResume();

  rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkLayerTest, GET) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent: Foo/1.0\r\n\r\n"),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                       "Foo/1.0");
  request_info.load_flags = LOAD_NORMAL;

  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(OK, rv);

  rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
  rv = callback.GetResult(rv);
  ASSERT_EQ(OK, rv);

  std::string contents;
  rv = ReadTransaction(trans.get(), &contents);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", contents);
}

// Proxy bypass tests. These tests run through various server-induced
// proxy bypass scenarios using both PAC file and fixed proxy params.
// The test scenarios are:
//  - bypass with two proxies configured and the first but not the second
//    is bypassed.
//  - bypass with one proxy configured and an explicit fallback to direct
//    connections
//  - bypass with two proxies configured and both are bypassed
//  - bypass with one proxy configured which is bypassed with no defined
//    fallback

#if defined(SPDY_PROXY_AUTH_ORIGIN)
TEST_F(HttpNetworkLayerTest, ServerTwoProxyBypassPac) {
  std::string bad_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + bad_proxy + "; PROXY good:8080"));
  TestProxyFallback(bad_proxy);
}

TEST_F(HttpNetworkLayerTest, ServerTwoProxyBypassFixed) {
  std::string bad_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(
      ProxyService::CreateFixed(bad_proxy +", good:8080"));
  TestProxyFallback(bad_proxy);
}

TEST_F(HttpNetworkLayerTest, BypassAndRetryIdempotentMethods) {
  std::string bad_proxy = GetDataReductionProxy();
  const struct {
    std::string method;
    std::string content;
    bool expected_to_retry;
  } tests[] = {
    {
      "GET",
      "content",
      true,
    },
    {
      "OPTIONS",
      "content",
      true,
    },
    {
      "HEAD",
      "",
      true,
    },
    {
      "PUT",
      "",
      true,
    },
    {
      "DELETE",
      "content",
      true,
    },
    {
      "TRACE",
      "content",
      true,
    },
    {
      "POST",
      "Bypass message",
      false,
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    ConfigureTestDependencies(
        ProxyService::CreateFixed(bad_proxy +", good:8080"));
    MockRead data_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Chrome-Proxy: bypass=0\r\n\r\n"),
      MockRead("Bypass message"),
      MockRead(SYNCHRONOUS, OK),
    };
    TestProxyFallbackByMethodWithMockReads(bad_proxy, "", data_reads,
                                           arraysize(data_reads),
                                           tests[i].method,
                                           tests[i].content,
                                           tests[i].expected_to_retry, 1u);
  }
}

TEST_F(HttpNetworkLayerTest, ServerOneProxyWithDirectBypassPac) {
  std::string bad_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + bad_proxy + "; DIRECT"));
  TestProxyFallbackToDirect(bad_proxy);
}

TEST_F(HttpNetworkLayerTest, ServerOneProxyWithDirectBypassFixed) {
  std::string bad_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(
      ProxyService::CreateFixed(bad_proxy + ", direct://"));
  TestProxyFallbackToDirect(bad_proxy);
}

#if defined(DATA_REDUCTION_FALLBACK_HOST)
TEST_F(HttpNetworkLayerTest, ServerTwoProxyDoubleBypassPac) {
  std::string bad_proxy = GetDataReductionProxy();
  std::string bad_proxy2 =
      HostPortPair::FromURL(GURL(DATA_REDUCTION_FALLBACK_HOST)).ToString();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + bad_proxy + "; PROXY " + bad_proxy2));
  TestProxyFallbackFail(2u, bad_proxy, bad_proxy2);
}

TEST_F(HttpNetworkLayerTest, ServerTwoProxyDoubleBypassFixed) {
  std::string bad_proxy = GetDataReductionProxy();
  std::string bad_proxy2 =
      HostPortPair::FromURL(GURL(DATA_REDUCTION_FALLBACK_HOST)).ToString();
  ConfigureTestDependencies(ProxyService::CreateFixed(
    bad_proxy + ", " + bad_proxy2));
  TestProxyFallbackFail(2u, bad_proxy, bad_proxy2);
}
#endif

TEST_F(HttpNetworkLayerTest, ServerOneProxyNoDirectBypassPac) {
  std::string bad_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + bad_proxy));
  TestProxyFallbackFail(1u, bad_proxy, "");
}

TEST_F(HttpNetworkLayerTest, ServerOneProxyNoDirectBypassFixed) {
  std::string bad_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixed(bad_proxy));
  TestProxyFallbackFail(1u, bad_proxy, "");
}

TEST_F(HttpNetworkLayerTest, ServerFallbackOn5xxError) {
  // Verify that "500 Internal Server Error", "502 Bad Gateway", and
  // "503 Service Unavailable" via the data reduction proxy induce proxy
  // fallback to a second proxy, if configured.

  // To configure this test, we need to wire up a custom proxy service to use
  // a pair of proxies. We'll induce fallback via the first and return
  // the expected data via the second.
  std::string data_reduction_proxy(
      HostPortPair::FromURL(GURL(SPDY_PROXY_AUTH_ORIGIN)).ToString());
  std::string pac_string = base::StringPrintf(
      "PROXY %s; PROXY good:8080", data_reduction_proxy.data());

  std::string headers[] = {
    "HTTP/1.1 500 Internal Server Error\r\n\r\n",
    "HTTP/1.1 502 Bad Gateway\r\n\r\n",
    "HTTP/1.1 503 Service Unavailable\r\n\r\n"
  };

  for (size_t i = 0; i < arraysize(headers); ++i) {
    ConfigureTestDependencies(
        ProxyService::CreateFixedFromPacResult(pac_string));

    MockRead data_reads[] = {
      MockRead(headers[i].c_str()),
      MockRead("Bypass message"),
      MockRead(SYNCHRONOUS, OK),
    };

    MockWrite data_writes[] = {
      MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
    };

    StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                   data_writes, arraysize(data_writes));
    mock_socket_factory_.AddSocketDataProvider(&data1);

    // Second data provider returns the expected content.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"
               "Server: not-proxy\r\n\r\n"),
      MockRead("content"),
      MockRead(SYNCHRONOUS, OK),
    };
    MockWrite data_writes2[] = {
      MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
    };

    StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                   data_writes2, arraysize(data_writes2));
    mock_socket_factory_.AddSocketDataProvider(&data2);

    TestCompletionCallback callback;

    HttpRequestInfo request_info;
    request_info.url = GURL("http://www.google.com/");
    request_info.method = "GET";
    request_info.load_flags = LOAD_NORMAL;

    scoped_ptr<HttpTransaction> trans;
    int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
    EXPECT_EQ(OK, rv);

    rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(OK, rv);

    std::string contents;
    rv = ReadTransaction(trans.get(), &contents);
    EXPECT_EQ(OK, rv);

    // We should obtain content from the second socket provider write
    // corresponding to the fallback proxy.
    EXPECT_EQ("content", contents);
    // We also have a server header here that isn't set by the proxy.
    EXPECT_TRUE(trans->GetResponseInfo()->headers->HasHeaderValue(
        "server", "not-proxy"));
    // We should also observe the data reduction proxy in the retry list.
    ASSERT_EQ(1u, proxy_service_->proxy_retry_info().size());
    EXPECT_EQ(data_reduction_proxy,
              (*proxy_service_->proxy_retry_info().begin()).first);
  }
}
#endif  // defined(SPDY_PROXY_AUTH_ORIGIN)

TEST_F(HttpNetworkLayerTest, ProxyBypassIgnoredOnDirectConnectionPac) {
  // Verify that a Chrome-Proxy header is ignored when returned from a directly
  // connected origin server.
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult("DIRECT"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Chrome-Proxy: bypass=0\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                 data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data1);
  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.load_flags = LOAD_NORMAL;

  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(OK, rv);

  rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(OK, rv);

  // We should have read the original page data.
  std::string contents;
  rv = ReadTransaction(trans.get(), &contents);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Bypass message", contents);

  // We should have no entries in our bad proxy list.
  ASSERT_EQ(0u, proxy_service_->proxy_retry_info().size());
}

#if defined(SPDY_PROXY_AUTH_ORIGIN)
TEST_F(HttpNetworkLayerTest, ServerFallbackWithProxyTimedBypass) {
  // Verify that a Chrome-Proxy: bypass=<seconds> header induces proxy
  // fallback to a second proxy, if configured.
  std::string bad_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + bad_proxy + "; PROXY good:8080"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Connection: keep-alive\r\n"
             "Chrome-Proxy: bypass=86400\r\n"
             "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };

  TestProxyFallbackWithMockReads(bad_proxy, "", data_reads,
                                 arraysize(data_reads), 1u);
  EXPECT_EQ(base::TimeDelta::FromSeconds(86400),
            (*proxy_service_->proxy_retry_info().begin()).second.current_delay);
}

TEST_F(HttpNetworkLayerTest, ServerFallbackWithWrongViaHeader) {
  // Verify that a Via header that lacks the Chrome-Proxy induces proxy fallback
  // to a second proxy, if configured.
  std::string chrome_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + chrome_proxy + "; PROXY good:8080"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Connection: keep-alive\r\n"
             "Via: 1.0 some-other-proxy\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };

  TestProxyFallbackWithMockReads(chrome_proxy, std::string(), data_reads,
                                 arraysize(data_reads), 1u);
}

TEST_F(HttpNetworkLayerTest, ServerFallbackWithNoViaHeader) {
  // Verify that the lack of a Via header induces proxy fallback to a second
  // proxy, if configured.
  std::string chrome_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + chrome_proxy + "; PROXY good:8080"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Connection: keep-alive\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };

  TestProxyFallbackWithMockReads(chrome_proxy, std::string(), data_reads,
                                 arraysize(data_reads), 1u);
}

TEST_F(HttpNetworkLayerTest, NoServerFallbackWith304Response) {
  // Verify that Chrome will not be induced to bypass the data reduction proxy
  // when the data reduction proxy via header is absent on a 304.
  std::string chrome_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + chrome_proxy + "; PROXY good:8080"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 304 Not Modified\r\n"
             "Connection: keep-alive\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  TestProxyFallbackByMethodWithMockReads(chrome_proxy, std::string(),
                                         data_reads, arraysize(data_reads),
                                         "GET", std::string(), false, 0);
}

TEST_F(HttpNetworkLayerTest, NoServerFallbackWithChainedViaHeader) {
  // Verify that Chrome will not be induced to bypass the data reduction proxy
  // when the data reduction proxy via header is present, even if that header
  // is chained.
  std::string chrome_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + chrome_proxy + "; PROXY good:8080"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Connection: keep-alive\r\n"
             "Via: 1.1 Chrome-Compression-Proxy, 1.0 some-other-proxy\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };

  TestProxyFallbackByMethodWithMockReads(chrome_proxy, std::string(),
                                         data_reads, arraysize(data_reads),
                                         "GET", "Bypass message", false, 0);
}

TEST_F(HttpNetworkLayerTest, NoServerFallbackWithDeprecatedViaHeader) {
  // Verify that Chrome will not be induced to bypass the data reduction proxy
  // when the deprecated data reduction proxy via header is present, even if
  // that header is chained.
  std::string chrome_proxy = GetDataReductionProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + chrome_proxy + "; PROXY good:8080"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Connection: keep-alive\r\n"
             "Via: 1.1 Chrome Compression Proxy\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };

  TestProxyFallbackByMethodWithMockReads(chrome_proxy, std::string(),
                                         data_reads, arraysize(data_reads),
                                         "GET", "Bypass message", false, 0);
}

#if defined(DATA_REDUCTION_FALLBACK_HOST)
TEST_F(HttpNetworkLayerTest, ServerFallbackWithProxyTimedBypassAll) {
  // Verify that a Chrome-Proxy: block=<seconds> header bypasses a
  // a configured Chrome-Proxy and fallback and induces proxy fallback to a
  // third proxy, if configured.
  std::string bad_proxy = GetDataReductionProxy();
  std::string fallback_proxy = GetChromeFallbackProxy();
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY " + bad_proxy + "; PROXY " + fallback_proxy +
      "; PROXY good:8080"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Connection: keep-alive\r\n"
             "Chrome-Proxy: block=86400\r\n"
             "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };

  TestProxyFallbackWithMockReads(bad_proxy, fallback_proxy, data_reads,
                                 arraysize(data_reads), 2u);
  EXPECT_EQ(base::TimeDelta::FromSeconds(86400),
            (*proxy_service_->proxy_retry_info().begin()).second.current_delay);
}
#endif  // defined(DATA_REDUCTION_FALLBACK_HOST)
#endif  // defined(SPDY_PROXY_AUTH_ORIGIN)

TEST_F(HttpNetworkLayerTest, NetworkVerified) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent: Foo/1.0\r\n\r\n"),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                       "Foo/1.0");
  request_info.load_flags = LOAD_NORMAL;

  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(OK, rv);

  rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
  ASSERT_EQ(OK, callback.GetResult(rv));

  EXPECT_TRUE(trans->GetResponseInfo()->network_accessed);
}

TEST_F(HttpNetworkLayerTest, NetworkUnVerified) {
  MockRead data_reads[] = {
    MockRead(ASYNC, ERR_CONNECTION_RESET),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent: Foo/1.0\r\n\r\n"),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                       "Foo/1.0");
  request_info.load_flags = LOAD_NORMAL;

  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(OK, rv);

  rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
  ASSERT_EQ(ERR_CONNECTION_RESET, callback.GetResult(rv));

  // If the response info is null, that means that any consumer won't
  // see the network accessed bit set.
  EXPECT_EQ(NULL, trans->GetResponseInfo());
}

}  // namespace

}  // namespace net
