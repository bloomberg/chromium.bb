// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/socket/transport_connect_job.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;
const char kGroupName[] = "a";

class SSLClientSocketPoolTest : public TestWithScopedTaskEnvironment {
 protected:
  SSLClientSocketPoolTest()
      : cert_verifier_(new MockCertVerifier),
        transport_security_state_(new TransportSecurityState),
        proxy_resolution_service_(ProxyResolutionService::CreateDirect()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        http_auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        http_server_properties_(new HttpServerPropertiesImpl),
        session_(CreateNetworkSession()),
        direct_transport_socket_params_(
            new TransportSocketParams(HostPortPair("host", 443),
                                      false,
                                      OnHostResolutionCallback())),
        transport_socket_pool_(kMaxSockets,
                               kMaxSocketsPerGroup,
                               &socket_factory_),
        proxy_transport_socket_params_(
            new TransportSocketParams(HostPortPair("proxy", 443),
                                      false,
                                      OnHostResolutionCallback())),
        http_proxy_socket_params_(
            new HttpProxySocketParams(proxy_transport_socket_params_,
                                      NULL,
                                      quic::QUIC_VERSION_UNSUPPORTED,
                                      std::string(),
                                      HostPortPair("host", 80),
                                      session_->http_auth_cache(),
                                      session_->http_auth_handler_factory(),
                                      session_->spdy_session_pool(),
                                      session_->quic_stream_factory(),
                                      /*is_trusted_proxy=*/false,
                                      /*tunnel=*/true,
                                      TRAFFIC_ANNOTATION_FOR_TESTS)),
        http_proxy_socket_pool_(kMaxSockets,
                                kMaxSocketsPerGroup,
                                &transport_socket_pool_,
                                NULL,
                                NULL,
                                NULL,
                                NULL) {
    ssl_config_service_->GetSSLConfig(&ssl_config_);
  }

  void CreatePool(bool transport_pool, bool http_proxy_pool) {
    pool_.reset(new SSLClientSocketPool(
        kMaxSockets, kMaxSocketsPerGroup, cert_verifier_.get(),
        NULL /* channel_id_service */, transport_security_state_.get(),
        &ct_verifier_, &ct_policy_enforcer_,
        nullptr /* ssl_client_session_cache */, &socket_factory_,
        transport_pool ? &transport_socket_pool_ : nullptr, nullptr,
        http_proxy_pool ? &http_proxy_socket_pool_ : nullptr,
        nullptr /* ssl_config_service */,
        nullptr /* network_quality_estimator */, nullptr /* net_log */));
  }

  scoped_refptr<SSLSocketParams> SSLParams(ProxyServer::Scheme proxy) {
    return base::MakeRefCounted<SSLSocketParams>(
        proxy == ProxyServer::SCHEME_DIRECT ? direct_transport_socket_params_
                                            : nullptr,
        nullptr,
        proxy == ProxyServer::SCHEME_HTTP ? http_proxy_socket_params_ : nullptr,
        HostPortPair("host", 443), ssl_config_, PRIVACY_MODE_DISABLED);
  }

  void AddAuthToCache() {
    const base::string16 kFoo(base::ASCIIToUTF16("foo"));
    const base::string16 kBar(base::ASCIIToUTF16("bar"));
    session_->http_auth_cache()->Add(GURL("http://proxy:443/"),
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     AuthCredentials(kFoo, kBar),
                                     "/");
  }

  HttpNetworkSession* CreateNetworkSession() {
    HttpNetworkSession::Context session_context;
    session_context.host_resolver = &host_resolver_;
    session_context.cert_verifier = cert_verifier_.get();
    session_context.transport_security_state = transport_security_state_.get();
    session_context.cert_transparency_verifier = &ct_verifier_;
    session_context.ct_policy_enforcer = &ct_policy_enforcer_;
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.client_socket_factory = &socket_factory_;
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.http_auth_handler_factory =
        http_auth_handler_factory_.get();
    session_context.http_server_properties = http_server_properties_.get();
    return new HttpNetworkSession(HttpNetworkSession::Params(),
                                  session_context);
  }

  MockClientSocketFactory socket_factory_;
  MockCachingHostResolver host_resolver_;
  std::unique_ptr<MockCertVerifier> cert_verifier_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  MultiLogCTVerifier ct_verifier_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  const std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  const std::unique_ptr<SSLConfigService> ssl_config_service_;
  const std::unique_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  const std::unique_ptr<HttpServerPropertiesImpl> http_server_properties_;
  const std::unique_ptr<HttpNetworkSession> session_;

  scoped_refptr<TransportSocketParams> direct_transport_socket_params_;
  MockTransportClientSocketPool transport_socket_pool_;

  scoped_refptr<TransportSocketParams> proxy_transport_socket_params_;

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_;
  HttpProxyClientSocketPool http_proxy_socket_pool_;

  SSLConfig ssl_config_;
  std::unique_ptr<SSLClientSocketPool> pool_;
};

TEST_F(SSLClientSocketPoolTest, DirectCertError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_COMMON_NAME_INVALID);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CERT_COMMON_NAME_INVALID));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, NeedProxyAuth) {
  MockWrite writes[] = {
      MockWrite(
          "CONNECT host:80 HTTP/1.1\r\n"
          "Host: host:80\r\n"
          "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
      MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10\r\n\r\n"),
      MockRead("0123456789"),
  };
  StaticSocketDataProvider data(reads, writes);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_PROXY_AUTH_REQUESTED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  const HttpResponseInfo& tunnel_info = handle.ssl_error_response_info();
  EXPECT_EQ(tunnel_info.headers->response_code(), 407);
  std::unique_ptr<ClientSocketHandle> tunnel_handle =
      handle.release_pending_http_proxy_connection();
  EXPECT_TRUE(tunnel_handle->socket());
  EXPECT_FALSE(tunnel_handle->socket()->IsConnected());
}

// It would be nice to also test the timeouts in SSLClientSocketPool.

// Test that SocketTag passed into SSLClientSocketPool is applied to returned
// sockets.
#if defined(OS_ANDROID)
TEST_F(SSLClientSocketPoolTest, Tag) {
  // Start test server.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, SSLServerConfig());
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  // TLS 1.3 sockets aren't reused until the read side has been pumped.
  // TODO(crbug.com/906668): Support pumping the read side and setting the
  // socket to be reusable.
  ssl_config_.version_max = SSL_PROTOCOL_VERSION_TLS1_2;

  TransportClientSocketPool tcp_pool(
      kMaxSockets, kMaxSocketsPerGroup,
      ClientSocketFactory::GetDefaultFactory(), &host_resolver_,
      nullptr /* cert_verifier */, nullptr /* channel_id_server */,
      nullptr /* transport_security_state */,
      nullptr /* cert_transparency_verifier */,
      nullptr /* ct_policy_enforcer */, nullptr /* ssl_client_session_cache */,
      std::string() /* ssl_session_cache_shard */,
      nullptr /* ssl_config_service */,
      nullptr /* socket_performance_watcher_factory */,
      nullptr /* network_quality_estimator */, nullptr /* netlog */);
  cert_verifier_->set_default_result(OK);
  SSLClientSocketPool pool(kMaxSockets, kMaxSocketsPerGroup,
                           cert_verifier_.get(), NULL /* channel_id_service */,
                           transport_security_state_.get(), &ct_verifier_,
                           &ct_policy_enforcer_,
                           nullptr /* ssl_client_session_cache */,
                           ClientSocketFactory::GetDefaultFactory(), &tcp_pool,
                           NULL, NULL, NULL, NULL, NULL);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);
  scoped_refptr<TransportSocketParams> tcp_params(new TransportSocketParams(
      test_server.host_port_pair(), false, OnHostResolutionCallback()));
  scoped_refptr<SSLSocketParams> params(
      new SSLSocketParams(tcp_params, NULL, NULL, test_server.host_port_pair(),
                          ssl_config_, PRIVACY_MODE_DISABLED));

  // Test socket is tagged before connected.
  uint64_t old_traffic = GetTaggedBytes(tag_val1);
  int rv = handle.Init(kGroupName, params, LOW, tag1,
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  // Test reused socket is retagged.
  StreamSocket* socket = handle.socket();
  handle.Reset();
  old_traffic = GetTaggedBytes(tag_val2);
  TestCompletionCallback callback2;
  rv = handle.Init(kGroupName, params, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  const char kRequest[] = "GET / HTTP/1.1\r\n\r\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv =
      handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                             callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1);
  rv = handle.socket()->Read(read_buffer.get(), read_buffer->size(),
                             callback.callback());
  EXPECT_EQ(read_buffer->size(), callback.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
  // Disconnect socket to prevent reuse.
  handle.socket()->Disconnect();
  handle.Reset();
}

TEST_F(SSLClientSocketPoolTest, TagTwoSockets) {
  // Start test server.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, SSLServerConfig());
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  TransportClientSocketPool tcp_pool(
      kMaxSockets, kMaxSocketsPerGroup,
      ClientSocketFactory::GetDefaultFactory(), &host_resolver_,
      nullptr /* cert_verifier */, nullptr /* channel_id_server */,
      nullptr /* transport_security_state */,
      nullptr /* cert_transparency_verifier */,
      nullptr /* ct_policy_enforcer */, nullptr /* ssl_client_session_cache */,
      std::string() /* ssl_session_cache_shard */,
      nullptr /* ssl_config_service */,
      nullptr /* socket_performance_watcher_factory */,
      nullptr /* network_quality_estimator */, nullptr /* netlog */);
  cert_verifier_->set_default_result(OK);
  SSLClientSocketPool pool(kMaxSockets, kMaxSocketsPerGroup,
                           cert_verifier_.get(), NULL /* channel_id_service */,
                           transport_security_state_.get(), &ct_verifier_,
                           &ct_policy_enforcer_,
                           nullptr /* ssl_client_session_cache */,
                           ClientSocketFactory::GetDefaultFactory(), &tcp_pool,
                           NULL, NULL, NULL, NULL, NULL);
  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);
  scoped_refptr<TransportSocketParams> tcp_params(new TransportSocketParams(
      test_server.host_port_pair(), false, OnHostResolutionCallback()));
  scoped_refptr<SSLSocketParams> params(
      new SSLSocketParams(tcp_params, NULL, NULL, test_server.host_port_pair(),
                          ssl_config_, PRIVACY_MODE_DISABLED));

  // Test connect jobs that are orphaned and then adopted, appropriately apply
  // new tag. Request socket with |tag1|.
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, LOW, tag1,
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), &pool, NetLogWithSource());
  EXPECT_TRUE(rv == OK || rv == ERR_IO_PENDING) << "Result: " << rv;
  // Abort and request socket with |tag2|.
  handle.Reset();
  TestCompletionCallback callback2;
  rv = handle.Init(kGroupName, params, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(callback2.GetResult(rv), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  // Verify socket has |tag2| applied.
  uint64_t old_traffic = GetTaggedBytes(tag_val2);
  const char kRequest[] = "GET / HTTP/1.1\r\n\r\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv = handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                              callback2.callback(),
                              TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback2.GetResult(rv));
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1);
  rv = handle.socket()->Read(read_buffer.get(), read_buffer->size(),
                             callback2.callback());
  EXPECT_EQ(read_buffer->size(), callback2.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
}

TEST_F(SSLClientSocketPoolTest, TagTwoSocketsFullPool) {
  // Start test server.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, SSLServerConfig());
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  TransportClientSocketPool tcp_pool(
      kMaxSockets, kMaxSocketsPerGroup,
      ClientSocketFactory::GetDefaultFactory(), &host_resolver_,
      nullptr /* cert_verifier */, nullptr /* channel_id_server */,
      nullptr /* transport_security_state */,
      nullptr /* cert_transparency_verifier */,
      nullptr /* ct_policy_enforcer */, nullptr /* ssl_client_session_cache */,
      std::string() /* ssl_session_cache_shard */,
      nullptr /* ssl_config_service */,
      nullptr /* socket_performance_watcher_factory */,
      nullptr /* network_quality_estimator */, nullptr /* netlog */);
  cert_verifier_->set_default_result(OK);
  SSLClientSocketPool pool(kMaxSockets, kMaxSocketsPerGroup,
                           cert_verifier_.get(), NULL /* channel_id_service */,
                           transport_security_state_.get(), &ct_verifier_,
                           &ct_policy_enforcer_,
                           nullptr /* ssl_client_session_cache */,
                           ClientSocketFactory::GetDefaultFactory(), &tcp_pool,
                           NULL, NULL, NULL, NULL, NULL);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);
  scoped_refptr<TransportSocketParams> tcp_params(new TransportSocketParams(
      test_server.host_port_pair(), false, OnHostResolutionCallback()));
  scoped_refptr<SSLSocketParams> params(
      new SSLSocketParams(tcp_params, NULL, NULL, test_server.host_port_pair(),
                          ssl_config_, PRIVACY_MODE_DISABLED));

  // Test that sockets paused by a full underlying socket pool are properly
  // connected and tagged when underlying pool is freed up.
  // Fill up all slots in TCP pool.
  ClientSocketHandle tcp_handles[kMaxSocketsPerGroup];
  int rv;
  for (auto& tcp_handle : tcp_handles) {
    rv = tcp_handle.Init(kGroupName,
                         TransportClientSocketPool::SocketParams::
                             CreateFromTransportSocketParams(tcp_params),
                         LOW, tag1, ClientSocketPool::RespectLimits::ENABLED,
                         callback.callback(), &tcp_pool, NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());
    EXPECT_TRUE(tcp_handle.socket());
    EXPECT_TRUE(tcp_handle.socket()->IsConnected());
  }
  // Request two SSL sockets.
  ClientSocketHandle handle_to_be_canceled;
  rv = handle_to_be_canceled.Init(
      kGroupName, params, LOW, tag1, ClientSocketPool::RespectLimits::ENABLED,
      callback.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = handle.Init(kGroupName, params, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Cancel first request.
  handle_to_be_canceled.Reset();
  // Disconnect a TCP socket to free up a slot.
  tcp_handles[0].socket()->Disconnect();
  tcp_handles[0].Reset();
  // Verify |handle| gets a valid tagged socket.
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  uint64_t old_traffic = GetTaggedBytes(tag_val2);
  const char kRequest[] = "GET / HTTP/1.1\r\n\r\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv =
      handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                             callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1);
  EXPECT_EQ(handle.socket()->Read(read_buffer.get(), read_buffer->size(),
                                  callback.callback()),
            ERR_IO_PENDING);
  EXPECT_THAT(callback.WaitForResult(), read_buffer->size());
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
}
#endif
}  // namespace

}  // namespace net
