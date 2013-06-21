// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
// This file can be included from net/http even though
// it is in net/websockets because it doesn't
// introduce any link dependency to net/websockets.
#include "net/websockets/websocket_stream_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class UseAlternateProtocolsScopedSetter {
 public:
  UseAlternateProtocolsScopedSetter(bool use_alternate_protocols)
      : use_alternate_protocols_(HttpStreamFactory::use_alternate_protocols()) {
    HttpStreamFactory::set_use_alternate_protocols(use_alternate_protocols);
  }
  ~UseAlternateProtocolsScopedSetter() {
    HttpStreamFactory::set_use_alternate_protocols(use_alternate_protocols_);
  }

 private:
  bool use_alternate_protocols_;
};

class MockWebSocketStream : public WebSocketStreamBase {
 public:
  enum StreamType {
    kStreamTypeBasic,
    kStreamTypeSpdy,
  };

  explicit MockWebSocketStream(StreamType type) : type_(type) {}

  virtual ~MockWebSocketStream() {}

  virtual WebSocketStream* AsWebSocketStream() OVERRIDE { return NULL; }

  StreamType type() const {
    return type_;
  }

 private:
  const StreamType type_;
};

// HttpStreamFactoryImpl subclass that can wait until a preconnect is complete.
class MockHttpStreamFactoryImplForPreconnect : public HttpStreamFactoryImpl {
 public:
  MockHttpStreamFactoryImplForPreconnect(HttpNetworkSession* session,
                                         bool for_websockets)
      : HttpStreamFactoryImpl(session, for_websockets),
        preconnect_done_(false),
        waiting_for_preconnect_(false) {}


  void WaitForPreconnects() {
    while (!preconnect_done_) {
      waiting_for_preconnect_ = true;
      base::MessageLoop::current()->Run();
      waiting_for_preconnect_ = false;
    }
  }

 private:
  // HttpStreamFactoryImpl methods.
  virtual void OnPreconnectsCompleteInternal() OVERRIDE {
    preconnect_done_ = true;
    if (waiting_for_preconnect_)
      base::MessageLoop::current()->Quit();
  }

  bool preconnect_done_;
  bool waiting_for_preconnect_;
};

class StreamRequestWaiter : public HttpStreamRequest::Delegate {
 public:
  StreamRequestWaiter()
      : waiting_for_stream_(false),
        stream_done_(false) {}

  // HttpStreamRequest::Delegate

  virtual void OnStreamReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      HttpStreamBase* stream) OVERRIDE {
    stream_done_ = true;
    if (waiting_for_stream_)
      base::MessageLoop::current()->Quit();
    stream_.reset(stream);
    used_ssl_config_ = used_ssl_config;
    used_proxy_info_ = used_proxy_info;
  }

  virtual void OnWebSocketStreamReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      WebSocketStreamBase* stream) OVERRIDE {
    stream_done_ = true;
    if (waiting_for_stream_)
      base::MessageLoop::current()->Quit();
    websocket_stream_.reset(stream);
    used_ssl_config_ = used_ssl_config;
    used_proxy_info_ = used_proxy_info;
  }

  virtual void OnStreamFailed(
      int status,
      const SSLConfig& used_ssl_config) OVERRIDE {}

  virtual void OnCertificateError(
      int status,
      const SSLConfig& used_ssl_config,
      const SSLInfo& ssl_info) OVERRIDE {}

  virtual void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                                const SSLConfig& used_ssl_config,
                                const ProxyInfo& used_proxy_info,
                                HttpAuthController* auth_controller) OVERRIDE {}

  virtual void OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                                 SSLCertRequestInfo* cert_info) OVERRIDE {}

  virtual void OnHttpsProxyTunnelResponse(const HttpResponseInfo& response_info,
                                          const SSLConfig& used_ssl_config,
                                          const ProxyInfo& used_proxy_info,
                                          HttpStreamBase* stream) OVERRIDE {}

  void WaitForStream() {
    while (!stream_done_) {
      waiting_for_stream_ = true;
      base::MessageLoop::current()->Run();
      waiting_for_stream_ = false;
    }
  }

  const SSLConfig& used_ssl_config() const {
    return used_ssl_config_;
  }

  const ProxyInfo& used_proxy_info() const {
    return used_proxy_info_;
  }

  HttpStreamBase* stream() {
    return stream_.get();
  }

  MockWebSocketStream* websocket_stream() {
    return static_cast<MockWebSocketStream*>(websocket_stream_.get());
  }

  bool stream_done() const { return stream_done_; }

 private:
  bool waiting_for_stream_;
  bool stream_done_;
  scoped_ptr<HttpStreamBase> stream_;
  scoped_ptr<WebSocketStreamBase> websocket_stream_;
  SSLConfig used_ssl_config_;
  ProxyInfo used_proxy_info_;

  DISALLOW_COPY_AND_ASSIGN(StreamRequestWaiter);
};

class WebSocketSpdyStream : public MockWebSocketStream {
 public:
  explicit WebSocketSpdyStream(SpdySession* spdy_session)
      : MockWebSocketStream(kStreamTypeSpdy), spdy_session_(spdy_session) {}

  virtual ~WebSocketSpdyStream() {}

  SpdySession* spdy_session() { return spdy_session_.get(); }

 private:
  scoped_refptr<SpdySession> spdy_session_;
};

class WebSocketBasicStream : public MockWebSocketStream {
 public:
  explicit WebSocketBasicStream(ClientSocketHandle* connection)
      : MockWebSocketStream(kStreamTypeBasic), connection_(connection) {}

  virtual ~WebSocketBasicStream() {
    connection_->socket()->Disconnect();
  }

  ClientSocketHandle* connection() { return connection_.get(); }

 private:
  scoped_ptr<ClientSocketHandle> connection_;
};

class WebSocketStreamFactory : public WebSocketStreamBase::Factory {
 public:
  virtual ~WebSocketStreamFactory() {}

  virtual WebSocketStreamBase* CreateBasicStream(ClientSocketHandle* connection,
                                                 bool using_proxy) OVERRIDE {
    return new WebSocketBasicStream(connection);
  }

  virtual WebSocketStreamBase* CreateSpdyStream(
      SpdySession* spdy_session,
      bool use_relative_url) OVERRIDE {
    return new WebSocketSpdyStream(spdy_session);
  }
};

struct SessionDependencies {
  // Custom proxy service dependency.
  explicit SessionDependencies(ProxyService* proxy_service)
      : host_resolver(new MockHostResolver),
        cert_verifier(new MockCertVerifier),
        transport_security_state(new TransportSecurityState),
        proxy_service(proxy_service),
        ssl_config_service(new SSLConfigServiceDefaults),
        http_auth_handler_factory(
            HttpAuthHandlerFactory::CreateDefault(host_resolver.get())),
        net_log(NULL) {}

  scoped_ptr<MockHostResolverBase> host_resolver;
  scoped_ptr<CertVerifier> cert_verifier;
  scoped_ptr<TransportSecurityState> transport_security_state;
  scoped_ptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  HttpServerPropertiesImpl http_server_properties;
  NetLog* net_log;
};

HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  HttpNetworkSession::Params params;
  params.host_resolver = session_deps->host_resolver.get();
  params.cert_verifier = session_deps->cert_verifier.get();
  params.transport_security_state =
      session_deps->transport_security_state.get();
  params.proxy_service = session_deps->proxy_service.get();
  params.ssl_config_service = session_deps->ssl_config_service.get();
  params.client_socket_factory = &session_deps->socket_factory;
  params.http_auth_handler_factory =
      session_deps->http_auth_handler_factory.get();
  params.net_log = session_deps->net_log;
  params.http_server_properties = &session_deps->http_server_properties;

  return new HttpNetworkSession(params);
}

struct TestCase {
  int num_streams;
  bool ssl;
};

TestCase kTests[] = {
  { 1, false },
  { 2, false },
  { 1, true},
  { 2, true},
};

void PreconnectHelperForURL(int num_streams,
                            const GURL& url,
                            HttpNetworkSession* session) {
  HttpNetworkSessionPeer peer(session);
  MockHttpStreamFactoryImplForPreconnect* mock_factory =
      new MockHttpStreamFactoryImplForPreconnect(session, false);
  peer.SetHttpStreamFactory(mock_factory);
  SSLConfig ssl_config;
  session->ssl_config_service()->GetSSLConfig(&ssl_config);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.load_flags = 0;

  session->http_stream_factory()->PreconnectStreams(
      num_streams, request, DEFAULT_PRIORITY, ssl_config, ssl_config);
  mock_factory->WaitForPreconnects();
};

void PreconnectHelper(const TestCase& test,
                      HttpNetworkSession* session) {
  GURL url = test.ssl ? GURL("https://www.google.com") :
      GURL("http://www.google.com");
  PreconnectHelperForURL(test.num_streams, url, session);
};

template<typename ParentPool>
class CapturePreconnectsSocketPool : public ParentPool {
 public:
  CapturePreconnectsSocketPool(HostResolver* host_resolver,
                               CertVerifier* cert_verifier);

  int last_num_streams() const {
    return last_num_streams_;
  }

  virtual int RequestSocket(const std::string& group_name,
                            const void* socket_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            const CompletionCallback& callback,
                            const BoundNetLog& net_log) OVERRIDE {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  virtual void RequestSockets(const std::string& group_name,
                              const void* socket_params,
                              int num_sockets,
                              const BoundNetLog& net_log) OVERRIDE {
    last_num_streams_ = num_sockets;
  }

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) OVERRIDE {
    ADD_FAILURE();
  }
  virtual void ReleaseSocket(const std::string& group_name,
                             StreamSocket* socket,
                             int id) OVERRIDE {
    ADD_FAILURE();
  }
  virtual void CloseIdleSockets() OVERRIDE {
    ADD_FAILURE();
  }
  virtual int IdleSocketCount() const OVERRIDE {
    ADD_FAILURE();
    return 0;
  }
  virtual int IdleSocketCountInGroup(
      const std::string& group_name) const OVERRIDE {
    ADD_FAILURE();
    return 0;
  }
  virtual LoadState GetLoadState(
      const std::string& group_name,
      const ClientSocketHandle* handle) const OVERRIDE {
    ADD_FAILURE();
    return LOAD_STATE_IDLE;
  }
  virtual base::TimeDelta ConnectionTimeout() const OVERRIDE {
    return base::TimeDelta();
  }

 private:
  int last_num_streams_;
};

typedef CapturePreconnectsSocketPool<TransportClientSocketPool>
CapturePreconnectsTransportSocketPool;
typedef CapturePreconnectsSocketPool<HttpProxyClientSocketPool>
CapturePreconnectsHttpProxySocketPool;
typedef CapturePreconnectsSocketPool<SOCKSClientSocketPool>
CapturePreconnectsSOCKSSocketPool;
typedef CapturePreconnectsSocketPool<SSLClientSocketPool>
CapturePreconnectsSSLSocketPool;

template<typename ParentPool>
CapturePreconnectsSocketPool<ParentPool>::CapturePreconnectsSocketPool(
    HostResolver* host_resolver, CertVerifier* /* cert_verifier */)
    : ParentPool(0, 0, NULL, host_resolver, NULL, NULL),
      last_num_streams_(-1) {}

template<>
CapturePreconnectsHttpProxySocketPool::CapturePreconnectsSocketPool(
    HostResolver* host_resolver, CertVerifier* /* cert_verifier */)
    : HttpProxyClientSocketPool(0, 0, NULL, host_resolver, NULL, NULL, NULL),
      last_num_streams_(-1) {}

template <>
CapturePreconnectsSSLSocketPool::CapturePreconnectsSocketPool(
    HostResolver* host_resolver,
    CertVerifier* cert_verifier)
    : SSLClientSocketPool(0,
                          0,
                          NULL,
                          host_resolver,
                          cert_verifier,
                          NULL,
                          NULL,
                          std::string(),
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL),
      last_num_streams_(-1) {}

TEST(HttpStreamFactoryTest, PreconnectDirect) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SessionDependencies session_deps(ProxyService::CreateDirect());
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);
    CapturePreconnectsTransportSocketPool* transport_conn_pool =
        new CapturePreconnectsTransportSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    MockClientSocketPoolManager* mock_pool_manager =
        new MockClientSocketPoolManager;
    mock_pool_manager->SetTransportSocketPool(transport_conn_pool);
    mock_pool_manager->SetSSLSocketPool(ssl_conn_pool);
    peer.SetClientSocketPoolManager(mock_pool_manager);
    PreconnectHelper(kTests[i], session.get());
    if (kTests[i].ssl)
      EXPECT_EQ(kTests[i].num_streams, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, transport_conn_pool->last_num_streams());
  }
}

TEST(HttpStreamFactoryTest, PreconnectHttpProxy) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SessionDependencies session_deps(ProxyService::CreateFixed("http_proxy"));
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);
    HostPortPair proxy_host("http_proxy", 80);
    CapturePreconnectsHttpProxySocketPool* http_proxy_pool =
        new CapturePreconnectsHttpProxySocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    MockClientSocketPoolManager* mock_pool_manager =
        new MockClientSocketPoolManager;
    mock_pool_manager->SetSocketPoolForHTTPProxy(proxy_host, http_proxy_pool);
    mock_pool_manager->SetSocketPoolForSSLWithProxy(proxy_host, ssl_conn_pool);
    peer.SetClientSocketPoolManager(mock_pool_manager);
    PreconnectHelper(kTests[i], session.get());
    if (kTests[i].ssl)
      EXPECT_EQ(kTests[i].num_streams, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, http_proxy_pool->last_num_streams());
  }
}

TEST(HttpStreamFactoryTest, PreconnectSocksProxy) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SessionDependencies session_deps(
        ProxyService::CreateFixed("socks4://socks_proxy:1080"));
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);
    HostPortPair proxy_host("socks_proxy", 1080);
    CapturePreconnectsSOCKSSocketPool* socks_proxy_pool =
        new CapturePreconnectsSOCKSSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    MockClientSocketPoolManager* mock_pool_manager =
        new MockClientSocketPoolManager;
    mock_pool_manager->SetSocketPoolForSOCKSProxy(proxy_host, socks_proxy_pool);
    mock_pool_manager->SetSocketPoolForSSLWithProxy(proxy_host, ssl_conn_pool);
    peer.SetClientSocketPoolManager(mock_pool_manager);
    PreconnectHelper(kTests[i], session.get());
    if (kTests[i].ssl)
      EXPECT_EQ(kTests[i].num_streams, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, socks_proxy_pool->last_num_streams());
  }
}

TEST(HttpStreamFactoryTest, PreconnectDirectWithExistingSpdySession) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SessionDependencies session_deps(ProxyService::CreateDirect());
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);

    // Set an existing SpdySession in the pool.
    HostPortPair host_port_pair("www.google.com", 443);
    SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                       kPrivacyModeDisabled);
    scoped_refptr<SpdySession> spdy_session =
        session->spdy_session_pool()->Get(key, BoundNetLog());

    CapturePreconnectsTransportSocketPool* transport_conn_pool =
        new CapturePreconnectsTransportSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(
            session_deps.host_resolver.get(),
            session_deps.cert_verifier.get());
    MockClientSocketPoolManager* mock_pool_manager =
        new MockClientSocketPoolManager;
    mock_pool_manager->SetTransportSocketPool(transport_conn_pool);
    mock_pool_manager->SetSSLSocketPool(ssl_conn_pool);
    peer.SetClientSocketPoolManager(mock_pool_manager);
    PreconnectHelper(kTests[i], session.get());
    // We shouldn't be preconnecting if we have an existing session, which is
    // the case for https://www.google.com.
    if (kTests[i].ssl)
      EXPECT_EQ(-1, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, transport_conn_pool->last_num_streams());
  }
}

// Verify that preconnects to unsafe ports are cancelled before they reach
// the SocketPool.
TEST(HttpStreamFactoryTest, PreconnectUnsafePort) {
  ASSERT_FALSE(IsPortAllowedByDefault(7));
  ASSERT_FALSE(IsPortAllowedByOverride(7));

  SessionDependencies session_deps(ProxyService::CreateDirect());
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
  HttpNetworkSessionPeer peer(session);
  CapturePreconnectsTransportSocketPool* transport_conn_pool =
      new CapturePreconnectsTransportSocketPool(
          session_deps.host_resolver.get(),
          session_deps.cert_verifier.get());
  MockClientSocketPoolManager* mock_pool_manager =
      new MockClientSocketPoolManager;
  mock_pool_manager->SetTransportSocketPool(transport_conn_pool);
  peer.SetClientSocketPoolManager(mock_pool_manager);

  PreconnectHelperForURL(1, GURL("http://www.google.com:7"), session.get());

  EXPECT_EQ(-1, transport_conn_pool->last_num_streams());
}

TEST(HttpStreamFactoryTest, JobNotifiesProxy) {
  const char* kProxyString = "PROXY bad:99; PROXY maybe:80; DIRECT";
  SessionDependencies session_deps(
      ProxyService::CreateFixedFromPacResult(kProxyString));

  // First connection attempt fails
  StaticSocketDataProvider socket_data1;
  socket_data1.set_connect_data(MockConnect(ASYNC, ERR_ADDRESS_UNREACHABLE));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data1);

  // Second connection attempt succeeds
  StaticSocketDataProvider socket_data2;
  socket_data2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data2);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Now request a stream. It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  // The proxy that failed should now be known to the proxy_service as bad.
  const ProxyRetryInfoMap& retry_info =
      session->proxy_service()->proxy_retry_info();
  EXPECT_EQ(1u, retry_info.size());
  ProxyRetryInfoMap::const_iterator iter = retry_info.find("bad:99");
  EXPECT_TRUE(iter != retry_info.end());
}

TEST(HttpStreamFactoryTest, PrivacyModeDisablesChannelId) {
  SessionDependencies session_deps(ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps.socket_factory.AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Set an existing SpdySession in the pool.
  HostPortPair host_port_pair("www.google.com", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     kPrivacyModeEnabled);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.privacy_mode = kPrivacyModeDisabled;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  // The stream shouldn't come from spdy as we are using different privacy mode
  EXPECT_FALSE(request->using_spdy());

  SSLConfig used_ssl_config = waiter.used_ssl_config();
  EXPECT_EQ(used_ssl_config.channel_id_enabled, ssl_config.channel_id_enabled);
}

namespace {
// Return count of distinct groups in given socket pool.
int GetSocketPoolGroupCount(ClientSocketPool* pool) {
  int count = 0;
  scoped_ptr<base::DictionaryValue> dict(pool->GetInfoAsValue("", "", false));
  EXPECT_TRUE(dict != NULL);
  base::DictionaryValue* groups = NULL;
  if (dict->GetDictionary("groups", &groups) && (groups != NULL)) {
    count = static_cast<int>(groups->size());
  }
  return count;
}
}  // namespace

TEST(HttpStreamFactoryTest, PrivacyModeUsesDifferentSocketPoolGroup) {
  SessionDependencies session_deps(ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps.socket_factory.AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
  SSLClientSocketPool* ssl_pool = session->GetSSLSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL);

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 0);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.privacy_mode = kPrivacyModeDisabled;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;

  scoped_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 1);

  scoped_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 1);

  request_info.privacy_mode = kPrivacyModeEnabled;
  scoped_ptr<HttpStreamRequest> request3(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));
  waiter.WaitForStream();

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 2);
}

TEST(HttpStreamFactoryTest, GetLoadState) {
  SessionDependencies session_deps(ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, ssl_config, ssl_config,
          &waiter, BoundNetLog()));

  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, request->GetLoadState());

  waiter.WaitForStream();
}

TEST(HttpStreamFactoryTest, RequestHttpStream) {
  SessionDependencies session_deps(ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Now request a stream.  It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(NULL != waiter.stream());
  EXPECT_TRUE(NULL == waiter.websocket_stream());
  EXPECT_FALSE(waiter.stream()->IsSpdyHttpStream());

  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSSLSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST(HttpStreamFactoryTest, RequestHttpStreamOverSSL) {
  SessionDependencies session_deps(ProxyService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(&mock_read, 1, NULL, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps.socket_factory.AddSSLSocketDataProvider(&ssl_socket_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(NULL != waiter.stream());
  EXPECT_TRUE(NULL == waiter.websocket_stream());
  EXPECT_FALSE(waiter.stream()->IsSpdyHttpStream());
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST(HttpStreamFactoryTest, RequestHttpStreamOverProxy) {
  SessionDependencies session_deps(ProxyService::CreateFixed("myproxy:8888"));

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Now request a stream.  It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(NULL != waiter.stream());
  EXPECT_TRUE(NULL == waiter.websocket_stream());
  EXPECT_FALSE(waiter.stream()->IsSpdyHttpStream());
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(session->GetSocketPoolForHTTPProxy(
      HttpNetworkSession::NORMAL_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPoolForSSLWithProxy(
      HttpNetworkSession::NORMAL_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_FALSE(waiter.used_proxy_info().is_direct());
}

TEST(HttpStreamFactoryTest, RequestWebSocketBasicStream) {
  SessionDependencies session_deps(ProxyService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("ws://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  WebSocketStreamFactory factory;
  scoped_ptr<HttpStreamRequest> request(
      session->websocket_stream_factory()->RequestWebSocketStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          &factory,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(NULL == waiter.stream());
  ASSERT_TRUE(NULL != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketStream::kStreamTypeBasic,
            waiter.websocket_stream()->type());
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST(HttpStreamFactoryTest, RequestWebSocketBasicStreamOverSSL) {
  SessionDependencies session_deps(ProxyService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(&mock_read, 1, NULL, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps.socket_factory.AddSSLSocketDataProvider(&ssl_socket_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("wss://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  WebSocketStreamFactory factory;
  scoped_ptr<HttpStreamRequest> request(
      session->websocket_stream_factory()->RequestWebSocketStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          &factory,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(NULL == waiter.stream());
  ASSERT_TRUE(NULL != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketStream::kStreamTypeBasic,
            waiter.websocket_stream()->type());
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST(HttpStreamFactoryTest, RequestWebSocketBasicStreamOverProxy) {
  SessionDependencies session_deps(ProxyService::CreateFixed("myproxy:8888"));

  MockRead read(SYNCHRONOUS, "HTTP/1.0 200 Connection established\r\n\r\n");
  StaticSocketDataProvider socket_data(&read, 1, 0, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory.AddSocketDataProvider(&socket_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("ws://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  WebSocketStreamFactory factory;
  scoped_ptr<HttpStreamRequest> request(
      session->websocket_stream_factory()->RequestWebSocketStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          &factory,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(NULL == waiter.stream());
  ASSERT_TRUE(NULL != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketStream::kStreamTypeBasic,
            waiter.websocket_stream()->type());
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(0, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(session->GetSocketPoolForHTTPProxy(
      HttpNetworkSession::NORMAL_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPoolForSSLWithProxy(
      HttpNetworkSession::NORMAL_SOCKET_POOL,
      HostPortPair("myproxy", 8888))));
  EXPECT_FALSE(waiter.used_proxy_info().is_direct());
}

TEST(HttpStreamFactoryTest, RequestSpdyHttpStream) {
  SpdySessionDependencies session_deps(kProtoSPDY3,
                                       ProxyService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(&mock_read, 1, NULL, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.SetNextProto(kProtoSPDY3);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  HostPortPair host_port_pair("www.google.com", 443);
  scoped_refptr<HttpNetworkSession>
      session(SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  scoped_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(NULL == waiter.websocket_stream());
  ASSERT_TRUE(NULL != waiter.stream());
  EXPECT_TRUE(waiter.stream()->IsSpdyHttpStream());
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST(HttpStreamFactoryTest, RequestWebSocketSpdyStream) {
  SpdySessionDependencies session_deps(kProtoSPDY3,
                                       ProxyService::CreateDirect());

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider socket_data(&mock_read, 1, NULL, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.SetNextProto(kProtoSPDY3);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  HostPortPair host_port_pair("www.google.com", 80);
  scoped_refptr<HttpNetworkSession>
      session(SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("wss://www.google.com");
  request_info.load_flags = 0;

  SSLConfig ssl_config;
  StreamRequestWaiter waiter1;
  WebSocketStreamFactory factory;
  scoped_ptr<HttpStreamRequest> request1(
      session->websocket_stream_factory()->RequestWebSocketStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter1,
          &factory,
          BoundNetLog()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  ASSERT_TRUE(NULL != waiter1.websocket_stream());
  EXPECT_EQ(MockWebSocketStream::kStreamTypeSpdy,
            waiter1.websocket_stream()->type());
  EXPECT_TRUE(NULL == waiter1.stream());

  StreamRequestWaiter waiter2;
  scoped_ptr<HttpStreamRequest> request2(
      session->websocket_stream_factory()->RequestWebSocketStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter2,
          &factory,
          BoundNetLog()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  ASSERT_TRUE(NULL != waiter2.websocket_stream());
  EXPECT_EQ(MockWebSocketStream::kStreamTypeSpdy,
            waiter2.websocket_stream()->type());
  EXPECT_TRUE(NULL == waiter2.stream());
  EXPECT_NE(waiter2.websocket_stream(), waiter1.websocket_stream());
  EXPECT_EQ(static_cast<WebSocketSpdyStream*>(waiter2.websocket_stream())->
            spdy_session(),
            static_cast<WebSocketSpdyStream*>(waiter1.websocket_stream())->
            spdy_session());

  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_TRUE(waiter1.used_proxy_info().is_direct());
}

TEST(HttpStreamFactoryTest, OrphanedWebSocketStream) {
  UseAlternateProtocolsScopedSetter use_alternate_protocols(true);
  SpdySessionDependencies session_deps(kProtoSPDY3,
                                       ProxyService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(&mock_read, 1, NULL, 0);
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  MockRead mock_read2(ASYNC, OK);
  StaticSocketDataProvider socket_data2(&mock_read2, 1, NULL, 0);
  socket_data2.set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data2);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.SetNextProto(kProtoSPDY3);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  scoped_refptr<HttpNetworkSession>
      session(SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("ws://www.google.com:8888");
  request_info.load_flags = 0;

  session->http_server_properties()->SetAlternateProtocol(
      HostPortPair("www.google.com", 8888),
      9999,
      NPN_SPDY_3);

  SSLConfig ssl_config;
  StreamRequestWaiter waiter;
  WebSocketStreamFactory factory;
  scoped_ptr<HttpStreamRequest> request(
      session->websocket_stream_factory()->RequestWebSocketStream(
          request_info,
          DEFAULT_PRIORITY,
          ssl_config,
          ssl_config,
          &waiter,
          &factory,
          BoundNetLog()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(NULL == waiter.stream());
  ASSERT_TRUE(NULL != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketStream::kStreamTypeSpdy,
            waiter.websocket_stream()->type());

  // Make sure that there was an alternative connection
  // which consumes extra connections.
  EXPECT_EQ(2, GetSocketPoolGroupCount(
      session->GetTransportSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_EQ(1, GetSocketPoolGroupCount(
      session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL)));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());

  // Make sure there is no orphaned job. it is already canceled.
  ASSERT_EQ(0u, static_cast<HttpStreamFactoryImpl*>(
      session->websocket_stream_factory())->num_orphaned_jobs());
}

}  // namespace

}  // namespace net
