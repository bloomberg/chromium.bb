// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_util.h"
#include "net/base/request_priority.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/default_channel_id_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TLS10SSLConfigService : public SSLConfigService {
 public:
  TLS10SSLConfigService() {
    ssl_config_.version_min = SSL_PROTOCOL_VERSION_TLS1;
    ssl_config_.version_max = SSL_PROTOCOL_VERSION_TLS1;
  }

  void GetSSLConfig(SSLConfig* config) override { *config = ssl_config_; }

 private:
  ~TLS10SSLConfigService() override {}

  SSLConfig ssl_config_;
};

class TLS12SSLConfigService : public SSLConfigService {
 public:
  TLS12SSLConfigService() {
    ssl_config_.version_min = SSL_PROTOCOL_VERSION_TLS1;
    ssl_config_.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  }

  void GetSSLConfig(SSLConfig* config) override { *config = ssl_config_; }

 private:
  ~TLS12SSLConfigService() override {}

  SSLConfig ssl_config_;
};

class TokenBindingSSLConfigService : public SSLConfigService {
 public:
  TokenBindingSSLConfigService() {
    ssl_config_.token_binding_params.push_back(TB_PARAM_ECDSAP256);
  }

  void GetSSLConfig(SSLConfig* config) override { *config = ssl_config_; }

 private:
  ~TokenBindingSSLConfigService() override {}

  SSLConfig ssl_config_;
};

}  // namespace

class HttpNetworkTransactionSSLTest : public testing::Test {
 protected:
  void SetUp() override {
    ssl_config_service_ = new TLS10SSLConfigService;
    session_params_.ssl_config_service = ssl_config_service_.get();

    auth_handler_factory_.reset(new HttpAuthHandlerMock::Factory());
    session_params_.http_auth_handler_factory = auth_handler_factory_.get();

    proxy_service_ = ProxyService::CreateDirect();
    session_params_.proxy_service = proxy_service_.get();

    session_params_.client_socket_factory = &mock_socket_factory_;
    session_params_.host_resolver = &mock_resolver_;
    session_params_.http_server_properties =
        http_server_properties_.GetWeakPtr();
    session_params_.transport_security_state = &transport_security_state_;
  }

  HttpRequestInfo* GetRequestInfo(const std::string& url) {
    HttpRequestInfo* request_info = new HttpRequestInfo;
    request_info->url = GURL(url);
    request_info->method = "GET";
    request_info_vector_.push_back(make_scoped_ptr(request_info));
    return request_info;
  }

  SSLConfig& GetServerSSLConfig(HttpNetworkTransaction* trans) {
    return trans->server_ssl_config_;
  }

  scoped_refptr<SSLConfigService> ssl_config_service_;
  scoped_ptr<HttpAuthHandlerMock::Factory> auth_handler_factory_;
  scoped_ptr<ProxyService> proxy_service_;

  MockClientSocketFactory mock_socket_factory_;
  MockHostResolver mock_resolver_;
  HttpServerPropertiesImpl http_server_properties_;
  TransportSecurityState transport_security_state_;
  HttpNetworkSession::Params session_params_;
  std::vector<scoped_ptr<HttpRequestInfo>> request_info_vector_;
};

// Tests that HttpNetworkTransaction attempts to fallback from
// TLS 1.2 to TLS 1.1, then from TLS 1.1 to TLS 1.0.
TEST_F(HttpNetworkTransactionSSLTest, SSLFallback) {
  ssl_config_service_ = new TLS12SSLConfigService;
  session_params_.ssl_config_service = ssl_config_service_.get();
  // |ssl_data1| is for the first handshake (TLS 1.2), which will fail
  // for protocol reasons (e.g., simulating a version rollback attack).
  SSLSocketDataProvider ssl_data1(ASYNC, ERR_SSL_PROTOCOL_ERROR);
  mock_socket_factory_.AddSSLSocketDataProvider(&ssl_data1);
  StaticSocketDataProvider data1(NULL, 0, NULL, 0);
  mock_socket_factory_.AddSocketDataProvider(&data1);

  // |ssl_data2| contains the handshake result for a TLS 1.1
  // handshake which will be attempted after the TLS 1.2
  // handshake fails.
  SSLSocketDataProvider ssl_data2(ASYNC, ERR_SSL_PROTOCOL_ERROR);
  mock_socket_factory_.AddSSLSocketDataProvider(&ssl_data2);
  StaticSocketDataProvider data2(NULL, 0, NULL, 0);
  mock_socket_factory_.AddSocketDataProvider(&data2);

  // |ssl_data3| contains the handshake result for a TLS 1.0
  // handshake which will be attempted after the TLS 1.1
  // handshake fails.
  SSLSocketDataProvider ssl_data3(ASYNC, ERR_SSL_PROTOCOL_ERROR);
  mock_socket_factory_.AddSSLSocketDataProvider(&ssl_data3);
  StaticSocketDataProvider data3(NULL, 0, NULL, 0);
  mock_socket_factory_.AddSocketDataProvider(&data3);

  HttpNetworkSession session(session_params_);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, &session);

  TestCompletionCallback callback;
  // This will consume |ssl_data1|, |ssl_data2| and |ssl_data3|.
  int rv =
      callback.GetResult(trans.Start(GetRequestInfo("https://www.paypal.com/"),
                                     callback.callback(), BoundNetLog()));
  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR, rv);

  SocketDataProviderArray<SocketDataProvider>& mock_data =
      mock_socket_factory_.mock_data();
  // Confirms that |ssl_data1|, |ssl_data2| and |ssl_data3| are consumed.
  EXPECT_EQ(3u, mock_data.next_index());

  SSLConfig& ssl_config = GetServerSSLConfig(&trans);
  // |version_max| fallbacks to TLS 1.0.
  EXPECT_EQ(SSL_PROTOCOL_VERSION_TLS1, ssl_config.version_max);
  EXPECT_TRUE(ssl_config.version_fallback);
}

#if !defined(OS_IOS)
TEST_F(HttpNetworkTransactionSSLTest, TokenBinding) {
  ssl_config_service_ = new TokenBindingSSLConfigService;
  session_params_.ssl_config_service = ssl_config_service_.get();
  ChannelIDService channel_id_service(new DefaultChannelIDStore(NULL),
                                      base::ThreadTaskRunnerHandle::Get());
  session_params_.channel_id_service = &channel_id_service;

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.token_binding_negotiated = true;
  ssl_data.token_binding_key_param = TB_PARAM_ECDSAP256;
  mock_socket_factory_.AddSSLSocketDataProvider(&ssl_data);
  MockRead mock_reads[] = {MockRead("HTTP/1.1 200 OK\r\n\r\n"),
                           MockRead(SYNCHRONOUS, OK)};
  StaticSocketDataProvider data(mock_reads, arraysize(mock_reads), NULL, 0);
  mock_socket_factory_.AddSocketDataProvider(&data);

  HttpNetworkSession session(session_params_);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, &session);

  TestCompletionCallback callback;
  int rv =
      callback.GetResult(trans.Start(GetRequestInfo("https://www.example.com/"),
                                     callback.callback(), BoundNetLog()));
  EXPECT_EQ(OK, rv);

  HttpRequestHeaders headers1;
  ASSERT_TRUE(trans.GetFullRequestHeaders(&headers1));
  std::string token_binding_header1;
  EXPECT_TRUE(headers1.GetHeader(HttpRequestHeaders::kTokenBinding,
                                 &token_binding_header1));

  // Send a second request and verify that the token binding header is the same
  // as in the first request.
  mock_socket_factory_.AddSSLSocketDataProvider(&ssl_data);
  StaticSocketDataProvider data2(mock_reads, arraysize(mock_reads), NULL, 0);
  mock_socket_factory_.AddSocketDataProvider(&data2);

  rv =
      callback.GetResult(trans.Start(GetRequestInfo("https://www.example.com/"),
                                     callback.callback(), BoundNetLog()));
  EXPECT_EQ(OK, rv);

  HttpRequestHeaders headers2;
  ASSERT_TRUE(trans.GetFullRequestHeaders(&headers2));
  std::string token_binding_header2;
  EXPECT_TRUE(headers2.GetHeader(HttpRequestHeaders::kTokenBinding,
                                 &token_binding_header2));

  EXPECT_EQ(token_binding_header1, token_binding_header2);
}
#endif  // !defined(OS_IOS)

}  // namespace net

