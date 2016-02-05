// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_test_util_common.h"

#include <stdint.h>
#include <cstddef>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "net/base/host_port_pair.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_stream.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace net {

namespace {

bool next_proto_is_spdy(NextProto next_proto) {
  return next_proto >= kProtoSPDYMinimumVersion &&
      next_proto <= kProtoSPDYMaximumVersion;
}

// Parses a URL into the scheme, host, and path components required for a
// SPDY request.
void ParseUrl(base::StringPiece url, std::string* scheme, std::string* host,
              std::string* path) {
  GURL gurl(url.as_string());
  path->assign(gurl.PathForRequest());
  scheme->assign(gurl.scheme());
  host->assign(gurl.host());
  if (gurl.has_port()) {
    host->append(":");
    host->append(gurl.port());
  }
}

}  // namespace

// Chop a frame into an array of MockWrites.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const SpdyFrame& frame, int num_chunks) {
  MockWrite* chunks = new MockWrite[num_chunks];
  int chunk_size = frame.size() / num_chunks;
  for (int index = 0; index < num_chunks; index++) {
    const char* ptr = frame.data() + (index * chunk_size);
    if (index == num_chunks - 1)
      chunk_size +=
          frame.size() % chunk_size;  // The last chunk takes the remainder.
    chunks[index] = MockWrite(ASYNC, ptr, chunk_size);
  }
  return chunks;
}

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendToHeaderBlock(const char* const extra_headers[],
                         int extra_header_count,
                         SpdyHeaderBlock* headers) {
  std::string this_header;
  std::string this_value;

  if (!extra_header_count)
    return;

  // Sanity check: Non-NULL header list.
  DCHECK(NULL != extra_headers) << "NULL header value pair list";
  // Sanity check: Non-NULL header map.
  DCHECK(NULL != headers) << "NULL header map";
  // Copy in the headers.
  for (int i = 0; i < extra_header_count; i++) {
    // Sanity check: Non-empty header.
    DCHECK_NE('\0', *extra_headers[i * 2]) << "Empty header value pair";
    this_header = extra_headers[i * 2];
    std::string::size_type header_len = this_header.length();
    if (!header_len)
      continue;
    this_value = extra_headers[1 + (i * 2)];
    std::string new_value;
    if (headers->find(this_header) != headers->end()) {
      // More than one entry in the header.
      // Don't add the header again, just the append to the value,
      // separated by a NULL character.

      // Adjust the value.
      new_value = (*headers)[this_header].as_string();
      // Put in a NULL separator.
      new_value.append(1, '\0');
      // Append the new value.
      new_value += this_value;
    } else {
      // Not a duplicate, just write the value.
      new_value = this_value;
    }
    (*headers)[this_header] = new_value;
  }
}

// Create a MockWrite from the given SpdyFrame.
MockWrite CreateMockWrite(const SpdyFrame& req) {
  return MockWrite(ASYNC, req.data(), req.size());
}

// Create a MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(const SpdyFrame& req, int seq) {
  return CreateMockWrite(req, seq, ASYNC);
}

// Create a MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(const SpdyFrame& req, int seq, IoMode mode) {
  return MockWrite(mode, req.data(), req.size(), seq);
}

// Create a MockRead from the given SpdyFrame.
MockRead CreateMockRead(const SpdyFrame& resp) {
  return MockRead(ASYNC, resp.data(), resp.size());
}

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(const SpdyFrame& resp, int seq) {
  return CreateMockRead(resp, seq, ASYNC);
}

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(const SpdyFrame& resp, int seq, IoMode mode) {
  return MockRead(mode, resp.data(), resp.size(), seq);
}

// Combines the given SpdyFrames into the given char array and returns
// the total length.
int CombineFrames(const SpdyFrame** frames,
                  int num_frames,
                  char* buf,
                  int buf_len) {
  int total_len = 0;
  for (int i = 0; i < num_frames; ++i) {
    total_len += frames[i]->size();
  }
  DCHECK_LE(total_len, buf_len);
  char* ptr = buf;
  for (int i = 0; i < num_frames; ++i) {
    int len = frames[i]->size();
    memcpy(ptr, frames[i]->data(), len);
    ptr += len;
  }
  return total_len;
}

namespace {

class PriorityGetter : public BufferedSpdyFramerVisitorInterface {
 public:
  PriorityGetter() : priority_(0) {}
  ~PriorityGetter() override {}

  SpdyPriority priority() const {
    return priority_;
  }

  void OnError(SpdyFramer::SpdyError error_code) override {}
  void OnStreamError(SpdyStreamId stream_id,
                     const std::string& description) override {}
  void OnSynStream(SpdyStreamId stream_id,
                   SpdyStreamId associated_stream_id,
                   SpdyPriority priority,
                   bool fin,
                   bool unidirectional,
                   const SpdyHeaderBlock& headers) override {
    priority_ = priority;
  }
  void OnSynReply(SpdyStreamId stream_id,
                  bool fin,
                  const SpdyHeaderBlock& headers) override {}
  void OnHeaders(SpdyStreamId stream_id,
                 bool has_priority,
                 SpdyPriority priority,
                 SpdyStreamId parent_stream_id,
                 bool exclusive,
                 bool fin,
                 const SpdyHeaderBlock& headers) override {
    if (has_priority) {
      priority_ = priority;
    }
  }
  void OnDataFrameHeader(SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override {}
  void OnStreamFrameData(SpdyStreamId stream_id,
                         const char* data,
                         size_t len,
                         bool fin) override {}
  void OnStreamPadding(SpdyStreamId stream_id, size_t len) override {}
  SpdyHeadersHandlerInterface* OnHeaderFrameStart(
      SpdyStreamId stream_id) override {
    return nullptr;
  }
  void OnHeaderFrameEnd(SpdyStreamId stream_id, bool end_headers) override {}
  void OnSettings(bool clear_persisted) override {}
  void OnSetting(SpdySettingsIds id, uint8_t flags, uint32_t value) override {}
  void OnPing(SpdyPingId unique_id, bool is_ack) override {}
  void OnRstStream(SpdyStreamId stream_id,
                   SpdyRstStreamStatus status) override {}
  void OnGoAway(SpdyStreamId last_accepted_stream_id,
                SpdyGoAwayStatus status,
                StringPiece debug_data) override {}
  void OnWindowUpdate(SpdyStreamId stream_id, int delta_window_size) override {}
  void OnPushPromise(SpdyStreamId stream_id,
                     SpdyStreamId promised_stream_id,
                     const SpdyHeaderBlock& headers) override {}
  bool OnUnknownFrame(SpdyStreamId stream_id, int frame_type) override {
    return false;
  }

 private:
  SpdyPriority priority_;
};

}  // namespace

bool GetSpdyPriority(SpdyMajorVersion version,
                     const SpdyFrame& frame,
                     SpdyPriority* priority) {
  BufferedSpdyFramer framer(version, false);
  PriorityGetter priority_getter;
  framer.set_visitor(&priority_getter);
  size_t frame_size = frame.size();
  if (framer.ProcessInput(frame.data(), frame_size) != frame_size) {
    return false;
  }
  *priority = priority_getter.priority();
  return true;
}

base::WeakPtr<SpdyStream> CreateStreamSynchronously(
    SpdyStreamType type,
    const base::WeakPtr<SpdySession>& session,
    const GURL& url,
    RequestPriority priority,
    const BoundNetLog& net_log) {
  SpdyStreamRequest stream_request;
  int rv = stream_request.StartRequest(type, session, url, priority, net_log,
                                       CompletionCallback());
  return
      (rv == OK) ? stream_request.ReleaseStream() : base::WeakPtr<SpdyStream>();
}

StreamReleaserCallback::StreamReleaserCallback() {}

StreamReleaserCallback::~StreamReleaserCallback() {}

CompletionCallback StreamReleaserCallback::MakeCallback(
    SpdyStreamRequest* request) {
  return base::Bind(&StreamReleaserCallback::OnComplete,
                    base::Unretained(this),
                    request);
}

void StreamReleaserCallback::OnComplete(
    SpdyStreamRequest* request, int result) {
  if (result == OK)
    request->ReleaseStream()->Cancel();
  SetResult(result);
}

MockECSignatureCreator::MockECSignatureCreator(crypto::ECPrivateKey* key)
    : key_(key) {
}

bool MockECSignatureCreator::Sign(const uint8_t* data,
                                  int data_len,
                                  std::vector<uint8_t>* signature) {
  std::vector<uint8_t> private_key_value;
  key_->ExportValue(&private_key_value);
  std::string head = "fakesignature";
  std::string tail = "/fakesignature";

  signature->clear();
  signature->insert(signature->end(), head.begin(), head.end());
  signature->insert(signature->end(), private_key_value.begin(),
                    private_key_value.end());
  signature->insert(signature->end(), '-');
  signature->insert(signature->end(), data, data + data_len);
  signature->insert(signature->end(), tail.begin(), tail.end());
  return true;
}

bool MockECSignatureCreator::DecodeSignature(
    const std::vector<uint8_t>& signature,
    std::vector<uint8_t>* out_raw_sig) {
  *out_raw_sig = signature;
  return true;
}

MockECSignatureCreatorFactory::MockECSignatureCreatorFactory() {
  crypto::ECSignatureCreator::SetFactoryForTesting(this);
}

MockECSignatureCreatorFactory::~MockECSignatureCreatorFactory() {
  crypto::ECSignatureCreator::SetFactoryForTesting(NULL);
}

crypto::ECSignatureCreator* MockECSignatureCreatorFactory::Create(
    crypto::ECPrivateKey* key) {
  return new MockECSignatureCreator(key);
}

SpdySessionDependencies::SpdySessionDependencies(NextProto protocol)
    : host_resolver(new MockCachingHostResolver),
      cert_verifier(new MockCertVerifier),
      channel_id_service(nullptr),
      transport_security_state(new TransportSecurityState),
      proxy_service(ProxyService::CreateDirect()),
      ssl_config_service(new SSLConfigServiceDefaults),
      socket_factory(new MockClientSocketFactory),
      http_auth_handler_factory(
          HttpAuthHandlerFactory::CreateDefault(host_resolver.get())),
      enable_ip_pooling(true),
      enable_compression(false),
      enable_ping(false),
      enable_user_alternate_protocol_ports(false),
      enable_npn(true),
      protocol(protocol),
      session_max_recv_window_size(
          SpdySession::GetDefaultInitialWindowSize(protocol)),
      stream_max_recv_window_size(
          SpdySession::GetDefaultInitialWindowSize(protocol)),
      time_func(&base::TimeTicks::Now),
      parse_alternative_services(false),
      enable_alternative_service_with_different_host(false),
      net_log(NULL) {
  DCHECK(next_proto_is_spdy(protocol)) << "Invalid protocol: " << protocol;

  // Note: The CancelledTransaction test does cleanup by running all
  // tasks in the message loop (RunAllPending).  Unfortunately, that
  // doesn't clean up tasks on the host resolver thread; and
  // TCPConnectJob is currently not cancellable.  Using synchronous
  // lookups allows the test to shutdown cleanly.  Until we have
  // cancellable TCPConnectJobs, use synchronous lookups.
  host_resolver->set_synchronous_mode(true);
}

SpdySessionDependencies::SpdySessionDependencies(
    NextProto protocol,
    scoped_ptr<ProxyService> proxy_service)
    : host_resolver(new MockHostResolver),
      cert_verifier(new MockCertVerifier),
      channel_id_service(nullptr),
      transport_security_state(new TransportSecurityState),
      proxy_service(std::move(proxy_service)),
      ssl_config_service(new SSLConfigServiceDefaults),
      socket_factory(new MockClientSocketFactory),
      http_auth_handler_factory(
          HttpAuthHandlerFactory::CreateDefault(host_resolver.get())),
      enable_ip_pooling(true),
      enable_compression(false),
      enable_ping(false),
      enable_user_alternate_protocol_ports(false),
      enable_npn(true),
      protocol(protocol),
      session_max_recv_window_size(
          SpdySession::GetDefaultInitialWindowSize(protocol)),
      stream_max_recv_window_size(
          SpdySession::GetDefaultInitialWindowSize(protocol)),
      time_func(&base::TimeTicks::Now),
      parse_alternative_services(false),
      enable_alternative_service_with_different_host(false),
      net_log(NULL) {
  DCHECK(next_proto_is_spdy(protocol)) << "Invalid protocol: " << protocol;
}

SpdySessionDependencies::~SpdySessionDependencies() {}

// static
scoped_ptr<HttpNetworkSession> SpdySessionDependencies::SpdyCreateSession(
    SpdySessionDependencies* session_deps) {
  HttpNetworkSession::Params params = CreateSessionParams(session_deps);
  params.client_socket_factory = session_deps->socket_factory.get();
  scoped_ptr<HttpNetworkSession> http_session(new HttpNetworkSession(params));
  SpdySessionPoolPeer pool_peer(http_session->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);
  return http_session;
}

// static
HttpNetworkSession::Params SpdySessionDependencies::CreateSessionParams(
    SpdySessionDependencies* session_deps) {
  DCHECK(next_proto_is_spdy(session_deps->protocol)) <<
      "Invalid protocol: " << session_deps->protocol;

  HttpNetworkSession::Params params;
  params.host_resolver = session_deps->host_resolver.get();
  params.cert_verifier = session_deps->cert_verifier.get();
  params.channel_id_service = session_deps->channel_id_service.get();
  params.transport_security_state =
      session_deps->transport_security_state.get();
  params.proxy_service = session_deps->proxy_service.get();
  params.ssl_config_service = session_deps->ssl_config_service.get();
  params.http_auth_handler_factory =
      session_deps->http_auth_handler_factory.get();
  params.http_server_properties =
      session_deps->http_server_properties.GetWeakPtr();
  params.enable_spdy_compression = session_deps->enable_compression;
  params.enable_spdy_ping_based_connection_checking = session_deps->enable_ping;
  params.enable_user_alternate_protocol_ports =
      session_deps->enable_user_alternate_protocol_ports;
  params.enable_npn = session_deps->enable_npn;
  params.spdy_default_protocol = session_deps->protocol;
  params.spdy_session_max_recv_window_size =
      session_deps->session_max_recv_window_size;
  params.spdy_stream_max_recv_window_size =
      session_deps->stream_max_recv_window_size;
  params.time_func = session_deps->time_func;
  params.proxy_delegate = session_deps->proxy_delegate.get();
  params.parse_alternative_services = session_deps->parse_alternative_services;
  params.enable_alternative_service_with_different_host =
      session_deps->enable_alternative_service_with_different_host;
  params.net_log = session_deps->net_log;
  return params;
}

SpdyURLRequestContext::SpdyURLRequestContext(NextProto protocol)
    : storage_(this) {
  DCHECK(next_proto_is_spdy(protocol)) << "Invalid protocol: " << protocol;

  storage_.set_host_resolver(scoped_ptr<HostResolver>(new MockHostResolver));
  storage_.set_cert_verifier(make_scoped_ptr(new MockCertVerifier));
  storage_.set_transport_security_state(
      make_scoped_ptr(new TransportSecurityState));
  storage_.set_proxy_service(ProxyService::CreateDirect());
  storage_.set_ssl_config_service(new SSLConfigServiceDefaults);
  storage_.set_http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault(host_resolver()));
  storage_.set_http_server_properties(
      scoped_ptr<HttpServerProperties>(new HttpServerPropertiesImpl()));
  storage_.set_job_factory(make_scoped_ptr(new URLRequestJobFactoryImpl()));
  HttpNetworkSession::Params params;
  params.client_socket_factory = &socket_factory_;
  params.host_resolver = host_resolver();
  params.cert_verifier = cert_verifier();
  params.transport_security_state = transport_security_state();
  params.proxy_service = proxy_service();
  params.ssl_config_service = ssl_config_service();
  params.http_auth_handler_factory = http_auth_handler_factory();
  params.network_delegate = network_delegate();
  params.enable_spdy_compression = false;
  params.enable_spdy_ping_based_connection_checking = false;
  params.spdy_default_protocol = protocol;
  params.http_server_properties = http_server_properties();
  storage_.set_http_network_session(
      make_scoped_ptr(new HttpNetworkSession(params)));
  SpdySessionPoolPeer pool_peer(
      storage_.http_network_session()->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);
  storage_.set_http_transaction_factory(make_scoped_ptr(
      new HttpCache(storage_.http_network_session(),
                    HttpCache::DefaultBackend::InMemory(0), false)));
}

SpdyURLRequestContext::~SpdyURLRequestContext() {
  AssertNoURLRequests();
}

bool HasSpdySession(SpdySessionPool* pool, const SpdySessionKey& key) {
  return pool->FindAvailableSession(key, BoundNetLog()) != NULL;
}

namespace {

base::WeakPtr<SpdySession> CreateSpdySessionHelper(
    HttpNetworkSession* http_session,
    const SpdySessionKey& key,
    const BoundNetLog& net_log,
    Error expected_status,
    bool is_secure) {
  EXPECT_FALSE(HasSpdySession(http_session->spdy_session_pool(), key));

  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(
          key.host_port_pair(), false, OnHostResolutionCallback(),
          TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT));

  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  TestCompletionCallback callback;

  int rv = ERR_UNEXPECTED;
  if (is_secure) {
    SSLConfig ssl_config;
    scoped_refptr<SSLSocketParams> ssl_params(
        new SSLSocketParams(transport_params,
                            NULL,
                            NULL,
                            key.host_port_pair(),
                            ssl_config,
                            key.privacy_mode(),
                            0,
                            false));
    rv = connection->Init(
        key.host_port_pair().ToString(), ssl_params, MEDIUM,
        ClientSocketPool::RespectLimits::ENABLED, callback.callback(),
        http_session->GetSSLSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL),
        net_log);
  } else {
    rv = connection->Init(key.host_port_pair().ToString(), transport_params,
                          MEDIUM, ClientSocketPool::RespectLimits::ENABLED,
                          callback.callback(),
                          http_session->GetTransportSocketPool(
                              HttpNetworkSession::NORMAL_SOCKET_POOL),
                          net_log);
  }

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(OK, rv);

  base::WeakPtr<SpdySession> spdy_session =
      http_session->spdy_session_pool()->CreateAvailableSessionFromSocket(
          key, std::move(connection), net_log, OK, is_secure);
  // Failure is reported asynchronously.
  EXPECT_TRUE(spdy_session != NULL);
  EXPECT_TRUE(HasSpdySession(http_session->spdy_session_pool(), key));
  return spdy_session;
}

}  // namespace

base::WeakPtr<SpdySession> CreateInsecureSpdySession(
    HttpNetworkSession* http_session,
    const SpdySessionKey& key,
    const BoundNetLog& net_log) {
  return CreateSpdySessionHelper(http_session, key, net_log,
                                 OK, false /* is_secure */);
}

base::WeakPtr<SpdySession> TryCreateInsecureSpdySessionExpectingFailure(
    HttpNetworkSession* http_session,
    const SpdySessionKey& key,
    Error expected_error,
    const BoundNetLog& net_log) {
  DCHECK_LT(expected_error, ERR_IO_PENDING);
  return CreateSpdySessionHelper(http_session, key, net_log,
                                 expected_error, false /* is_secure */);
}

base::WeakPtr<SpdySession> CreateSecureSpdySession(
    HttpNetworkSession* http_session,
    const SpdySessionKey& key,
    const BoundNetLog& net_log) {
  return CreateSpdySessionHelper(http_session, key, net_log,
                                 OK, true /* is_secure */);
}

namespace {

// A ClientSocket used for CreateFakeSpdySession() below.
class FakeSpdySessionClientSocket : public MockClientSocket {
 public:
  explicit FakeSpdySessionClientSocket(int read_result)
      : MockClientSocket(BoundNetLog()), read_result_(read_result) {}

  ~FakeSpdySessionClientSocket() override {}

  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override {
    return read_result_;
  }

  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  // Return kProtoUnknown to use the pool's default protocol.
  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }

  // The functions below are not expected to be called.

  int Connect(const CompletionCallback& callback) override {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  bool WasEverUsed() const override {
    ADD_FAILURE();
    return false;
  }

  bool UsingTCPFastOpen() const override {
    ADD_FAILURE();
    return false;
  }

  bool WasNpnNegotiated() const override {
    ADD_FAILURE();
    return false;
  }

  bool GetSSLInfo(SSLInfo* ssl_info) override {
    ADD_FAILURE();
    return false;
  }

  int64_t GetTotalReceivedBytes() const override {
    NOTIMPLEMENTED();
    return 0;
  }

 private:
  int read_result_;
};

base::WeakPtr<SpdySession> CreateFakeSpdySessionHelper(
    SpdySessionPool* pool,
    const SpdySessionKey& key,
    Error expected_status) {
  EXPECT_NE(expected_status, ERR_IO_PENDING);
  EXPECT_FALSE(HasSpdySession(pool, key));
  scoped_ptr<ClientSocketHandle> handle(new ClientSocketHandle());
  handle->SetSocket(scoped_ptr<StreamSocket>(new FakeSpdySessionClientSocket(
      expected_status == OK ? ERR_IO_PENDING : expected_status)));
  base::WeakPtr<SpdySession> spdy_session =
      pool->CreateAvailableSessionFromSocket(
          key, std::move(handle), BoundNetLog(), OK, true /* is_secure */);
  // Failure is reported asynchronously.
  EXPECT_TRUE(spdy_session != NULL);
  EXPECT_TRUE(HasSpdySession(pool, key));
  return spdy_session;
}

}  // namespace

base::WeakPtr<SpdySession> CreateFakeSpdySession(SpdySessionPool* pool,
                                                 const SpdySessionKey& key) {
  return CreateFakeSpdySessionHelper(pool, key, OK);
}

base::WeakPtr<SpdySession> TryCreateFakeSpdySessionExpectingFailure(
    SpdySessionPool* pool,
    const SpdySessionKey& key,
    Error expected_error) {
  DCHECK_LT(expected_error, ERR_IO_PENDING);
  return CreateFakeSpdySessionHelper(pool, key, expected_error);
}

SpdySessionPoolPeer::SpdySessionPoolPeer(SpdySessionPool* pool) : pool_(pool) {
}

void SpdySessionPoolPeer::RemoveAliases(const SpdySessionKey& key) {
  pool_->RemoveAliases(key);
}

void SpdySessionPoolPeer::DisableDomainAuthenticationVerification() {
  pool_->verify_domain_authentication_ = false;
}

void SpdySessionPoolPeer::SetEnableSendingInitialData(bool enabled) {
  pool_->enable_sending_initial_data_ = enabled;
}

void SpdySessionPoolPeer::SetSessionMaxRecvWindowSize(size_t window) {
  pool_->session_max_recv_window_size_ = window;
}

void SpdySessionPoolPeer::SetStreamInitialRecvWindowSize(size_t window) {
  pool_->stream_max_recv_window_size_ = window;
}

SpdyTestUtil::SpdyTestUtil(NextProto protocol, bool dependency_priorities)
    : protocol_(protocol),
      spdy_version_(NextProtoToSpdyMajorVersion(protocol)),
      default_url_(GURL(kDefaultURL)),
      dependency_priorities_(dependency_priorities) {
  DCHECK(next_proto_is_spdy(protocol)) << "Invalid protocol: " << protocol;
}

SpdyTestUtil::~SpdyTestUtil() {}

void SpdyTestUtil::AddUrlToHeaderBlock(base::StringPiece url,
                                       SpdyHeaderBlock* headers) const {
  std::string scheme, host, path;
  ParseUrl(url, &scheme, &host, &path);
  (*headers)[GetHostKey()] = host;
  (*headers)[GetSchemeKey()] = scheme;
  (*headers)[GetPathKey()] = path;
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructGetHeaderBlock(
    base::StringPiece url) const {
  return ConstructHeaderBlock("GET", url, NULL);
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructGetHeaderBlockForProxy(
    base::StringPiece url) const {
  scoped_ptr<SpdyHeaderBlock> headers(ConstructGetHeaderBlock(url));
  return headers;
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructHeadHeaderBlock(
    base::StringPiece url,
    int64_t content_length) const {
  return ConstructHeaderBlock("HEAD", url, nullptr);
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructPostHeaderBlock(
    base::StringPiece url,
    int64_t content_length) const {
  return ConstructHeaderBlock("POST", url, &content_length);
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructPutHeaderBlock(
    base::StringPiece url,
    int64_t content_length) const {
  return ConstructHeaderBlock("PUT", url, &content_length);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyFrame(
    const SpdyHeaderInfo& header_info,
    scoped_ptr<SpdyHeaderBlock> headers) const {
  BufferedSpdyFramer framer(spdy_version_, header_info.compressed);
  SpdyFrame* frame = NULL;
  switch (header_info.kind) {
    case DATA:
      frame = framer.CreateDataFrame(header_info.id, header_info.data,
                                     header_info.data_length,
                                     header_info.data_flags);
      break;
    case SYN_STREAM:
      {
        frame = framer.CreateSynStream(header_info.id, header_info.assoc_id,
                                       header_info.priority,
                                       header_info.control_flags,
                                       headers.get());
      }
      break;
    case SYN_REPLY:
      frame = framer.CreateSynReply(header_info.id, header_info.control_flags,
                                    headers.get());
      break;
    case RST_STREAM:
      frame = framer.CreateRstStream(header_info.id, header_info.status);
      break;
    case HEADERS:
      frame = framer.CreateHeaders(header_info.id, header_info.control_flags,
                                   header_info.priority,
                                   headers.get());
      break;
    default:
      ADD_FAILURE();
      break;
  }
  return frame;
}

SpdyFrame* SpdyTestUtil::ConstructSpdyFrame(const SpdyHeaderInfo& header_info,
                                            const char* const extra_headers[],
                                            int extra_header_count,
                                            const char* const tail_headers[],
                                            int tail_header_count) const {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  AppendToHeaderBlock(extra_headers, extra_header_count, headers.get());
  if (tail_headers && tail_header_count)
    AppendToHeaderBlock(tail_headers, tail_header_count, headers.get());
  return ConstructSpdyFrame(header_info, std::move(headers));
}

SpdyFrame* SpdyTestUtil::ConstructSpdyControlFrame(
    scoped_ptr<SpdyHeaderBlock> headers,
    bool compressed,
    SpdyStreamId stream_id,
    RequestPriority request_priority,
    SpdyFrameType type,
    SpdyControlFlags flags,
    SpdyStreamId associated_stream_id) const {
  EXPECT_GE(type, DATA);
  EXPECT_LE(type, PRIORITY);
  const SpdyHeaderInfo header_info = {
    type,
    stream_id,
    associated_stream_id,
    ConvertRequestPriorityToSpdyPriority(request_priority, spdy_version_),
    flags,
    compressed,
    RST_STREAM_INVALID,  // status
    NULL,  // data
    0,  // length
    DATA_FLAG_NONE
  };
  return ConstructSpdyFrame(header_info, std::move(headers));
}

SpdyFrame* SpdyTestUtil::ConstructSpdyControlFrame(
    const char* const extra_headers[],
    int extra_header_count,
    bool compressed,
    SpdyStreamId stream_id,
    RequestPriority request_priority,
    SpdyFrameType type,
    SpdyControlFlags flags,
    const char* const* tail_headers,
    int tail_header_size,
    SpdyStreamId associated_stream_id) const {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  AppendToHeaderBlock(extra_headers, extra_header_count, headers.get());
  if (tail_headers && tail_header_size)
    AppendToHeaderBlock(tail_headers, tail_header_size / 2, headers.get());
  return ConstructSpdyControlFrame(std::move(headers), compressed, stream_id,
                                   request_priority, type, flags,
                                   associated_stream_id);
}

std::string SpdyTestUtil::ConstructSpdyReplyString(
    const SpdyHeaderBlock& headers) const {
  std::string reply_string;
  for (SpdyHeaderBlock::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    std::string key = it->first.as_string();
    // Remove leading colon from "special" headers (for SPDY3 and
    // above).
    if (spdy_version() >= SPDY3 && key[0] == ':')
      key = key.substr(1);
    for (const std::string& value :
         base::SplitString(it->second, base::StringPiece("\0", 1),
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      reply_string += key + ": " + value + "\n";
    }
  }
  return reply_string;
}

// TODO(jgraettinger): Eliminate uses of this method in tests (prefer
// SpdySettingsIR).
SpdyFrame* SpdyTestUtil::ConstructSpdySettings(
    const SettingsMap& settings) const {
  SpdySettingsIR settings_ir;
  for (SettingsMap::const_iterator it = settings.begin();
       it != settings.end();
       ++it) {
    settings_ir.AddSetting(
        it->first,
        (it->second.first & SETTINGS_FLAG_PLEASE_PERSIST) != 0,
        (it->second.first & SETTINGS_FLAG_PERSISTED) != 0,
        it->second.second);
  }
  return CreateFramer(false)->SerializeFrame(settings_ir);
}

SpdyFrame* SpdyTestUtil::ConstructSpdySettingsAck() const {
  char kEmptyWrite[] = "";

  if (spdy_version() > SPDY3) {
    SpdySettingsIR settings_ir;
    settings_ir.set_is_ack(true);
    return CreateFramer(false)->SerializeFrame(settings_ir);
  }
  // No settings ACK write occurs. Create an empty placeholder write.
  return new SpdyFrame(kEmptyWrite, 0, false);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPing(uint32_t ping_id,
                                           bool is_ack) const {
  SpdyPingIR ping_ir(ping_id);
  ping_ir.set_is_ack(is_ack);
  return CreateFramer(false)->SerializeFrame(ping_ir);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGoAway() const {
  return ConstructSpdyGoAway(0);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGoAway(
    SpdyStreamId last_good_stream_id) const {
  SpdyGoAwayIR go_ir(last_good_stream_id, GOAWAY_OK, "go away");
  return CreateFramer(false)->SerializeFrame(go_ir);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGoAway(SpdyStreamId last_good_stream_id,
                                             SpdyGoAwayStatus status,
                                             const std::string& desc) const {
  SpdyGoAwayIR go_ir(last_good_stream_id, status, desc);
  return CreateFramer(false)->SerializeFrame(go_ir);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyWindowUpdate(
    const SpdyStreamId stream_id,
    uint32_t delta_window_size) const {
  SpdyWindowUpdateIR update_ir(stream_id, delta_window_size);
  return CreateFramer(false)->SerializeFrame(update_ir);
}

// TODO(jgraettinger): Eliminate uses of this method in tests (prefer
// SpdyRstStreamIR).
SpdyFrame* SpdyTestUtil::ConstructSpdyRstStream(
    SpdyStreamId stream_id,
    SpdyRstStreamStatus status) const {
  SpdyRstStreamIR rst_ir(stream_id, status);
  return CreateFramer(false)->SerializeRstStream(rst_ir);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGet(const char* const url,
                                          bool compressed,
                                          SpdyStreamId stream_id,
                                          RequestPriority request_priority) {
  scoped_ptr<SpdyHeaderBlock> block(ConstructGetHeaderBlock(url));
  return ConstructSpdySyn(
      stream_id, *block, request_priority, compressed, true);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGet(const char* const extra_headers[],
                                          int extra_header_count,
                                          bool compressed,
                                          int stream_id,
                                          RequestPriority request_priority,
                                          bool direct) {
  SpdyHeaderBlock block;
  MaybeAddVersionHeader(&block);
  block[GetMethodKey()] = "GET";
  AddUrlToHeaderBlock(default_url_.spec(), &block);
  AppendToHeaderBlock(extra_headers, extra_header_count, &block);
  return ConstructSpdySyn(stream_id, block, request_priority, compressed, true);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyConnect(
    const char* const extra_headers[],
    int extra_header_count,
    int stream_id,
    RequestPriority priority,
    const HostPortPair& host_port_pair) {
  SpdyHeaderBlock block;
  MaybeAddVersionHeader(&block);
  block[GetMethodKey()] = "CONNECT";
  if (spdy_version() < HTTP2) {
    block[GetHostKey()] = (host_port_pair.port() == 443)
                              ? host_port_pair.host()
                              : host_port_pair.ToString();
    block[GetPathKey()] = host_port_pair.ToString();
  } else {
    block[GetHostKey()] = host_port_pair.ToString();
  }
  AppendToHeaderBlock(extra_headers, extra_header_count, &block);
  return ConstructSpdySyn(stream_id, block, priority, false, false);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPush(const char* const extra_headers[],
                                           int extra_header_count,
                                           int stream_id,
                                           int associated_stream_id,
                                           const char* url) {
  if (spdy_version() < HTTP2) {
    SpdySynStreamIR syn_stream(stream_id);
    syn_stream.set_associated_to_stream_id(associated_stream_id);
    syn_stream.SetHeader("hello", "bye");
    syn_stream.SetHeader(GetStatusKey(), "200");
    syn_stream.SetHeader(GetVersionKey(), "HTTP/1.1");
    AddUrlToHeaderBlock(url, syn_stream.mutable_header_block());
    AppendToHeaderBlock(extra_headers, extra_header_count,
                        syn_stream.mutable_header_block());
    return CreateFramer(false)->SerializeFrame(syn_stream);
  } else {
    SpdyPushPromiseIR push_promise(associated_stream_id, stream_id);
    AddUrlToHeaderBlock(url, push_promise.mutable_header_block());
    scoped_ptr<SpdyFrame> push_promise_frame(
        CreateFramer(false)->SerializeFrame(push_promise));

    SpdyHeadersIR headers(stream_id);
    headers.SetHeader(GetStatusKey(), "200");
    headers.SetHeader("hello", "bye");
    AppendToHeaderBlock(extra_headers, extra_header_count,
                        headers.mutable_header_block());
    scoped_ptr<SpdyFrame> headers_frame(
        CreateFramer(false)->SerializeFrame(headers));

    int joint_data_size = push_promise_frame->size() + headers_frame->size();
    scoped_ptr<char[]> data(new char[joint_data_size]);
    const SpdyFrame* frames[2] = {
        push_promise_frame.get(), headers_frame.get(),
    };
    int combined_size =
        CombineFrames(frames, arraysize(frames), data.get(), joint_data_size);
    DCHECK_EQ(combined_size, joint_data_size);
    return new SpdyFrame(data.release(), joint_data_size, true);
  }
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPush(const char* const extra_headers[],
                                           int extra_header_count,
                                           int stream_id,
                                           int associated_stream_id,
                                           const char* url,
                                           const char* status,
                                           const char* location) {
  if (spdy_version() < HTTP2) {
    SpdySynStreamIR syn_stream(stream_id);
    syn_stream.set_associated_to_stream_id(associated_stream_id);
    syn_stream.SetHeader("hello", "bye");
    syn_stream.SetHeader(GetStatusKey(), status);
    syn_stream.SetHeader(GetVersionKey(), "HTTP/1.1");
    syn_stream.SetHeader("location", location);
    AddUrlToHeaderBlock(url, syn_stream.mutable_header_block());
    AppendToHeaderBlock(extra_headers, extra_header_count,
                        syn_stream.mutable_header_block());
    return CreateFramer(false)->SerializeFrame(syn_stream);
  } else {
    SpdyPushPromiseIR push_promise(associated_stream_id, stream_id);
    AddUrlToHeaderBlock(url, push_promise.mutable_header_block());
    scoped_ptr<SpdyFrame> push_promise_frame(
        CreateFramer(false)->SerializeFrame(push_promise));

    SpdyHeadersIR headers(stream_id);
    headers.SetHeader("hello", "bye");
    headers.SetHeader(GetStatusKey(), status);
    headers.SetHeader("location", location);
    AppendToHeaderBlock(extra_headers, extra_header_count,
                        headers.mutable_header_block());
    scoped_ptr<SpdyFrame> headers_frame(
        CreateFramer(false)->SerializeFrame(headers));

    int joint_data_size = push_promise_frame->size() + headers_frame->size();
    scoped_ptr<char[]> data(new char[joint_data_size]);
    const SpdyFrame* frames[2] = {
        push_promise_frame.get(), headers_frame.get(),
    };
    int combined_size =
        CombineFrames(frames, arraysize(frames), data.get(), joint_data_size);
    DCHECK_EQ(combined_size, joint_data_size);
    return new SpdyFrame(data.release(), joint_data_size, true);
  }
}

SpdyFrame* SpdyTestUtil::ConstructInitialSpdyPushFrame(
    scoped_ptr<SpdyHeaderBlock> headers,
    int stream_id,
    int associated_stream_id) {
  if (spdy_version() < HTTP2) {
    SpdySynStreamIR syn_stream(stream_id);
    syn_stream.set_associated_to_stream_id(associated_stream_id);
    SetPriority(LOWEST, &syn_stream);
    syn_stream.set_header_block(*headers);
    return CreateFramer(false)->SerializeFrame(syn_stream);
  } else {
    SpdyPushPromiseIR push_promise(associated_stream_id, stream_id);
    push_promise.set_header_block(*headers);
    return CreateFramer(false)->SerializeFrame(push_promise);
  }
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPushHeaders(
    int stream_id,
    const char* const extra_headers[],
    int extra_header_count) {
  SpdyHeadersIR headers(stream_id);
  headers.SetHeader(GetStatusKey(), "200");
  MaybeAddVersionHeader(&headers);
  AppendToHeaderBlock(extra_headers, extra_header_count,
                      headers.mutable_header_block());
  return CreateFramer(false)->SerializeFrame(headers);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyHeaderFrame(int stream_id,
                                                  const char* const headers[],
                                                  int header_count) {
  return ConstructSpdyHeaderFrame(stream_id, headers, header_count, false);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyHeaderFrame(int stream_id,
                                                  const char* const headers[],
                                                  int header_count,
                                                  bool fin) {
  SpdyHeadersIR spdy_headers(stream_id);
  spdy_headers.set_fin(fin);
  AppendToHeaderBlock(headers, header_count,
                      spdy_headers.mutable_header_block());
  return CreateFramer(false)->SerializeFrame(spdy_headers);
}

SpdyFrame* SpdyTestUtil::ConstructSpdySyn(int stream_id,
                                          const SpdyHeaderBlock& block,
                                          RequestPriority priority,
                                          bool compressed,
                                          bool fin) {
  // Get the stream id of the next highest priority request
  // (most recent request of the same priority, or last request of
  // an earlier priority).
  int parent_stream_id = 0;
  for (int q = priority; q >= IDLE; --q) {
    if (!priority_to_stream_id_list_[q].empty()) {
      parent_stream_id = priority_to_stream_id_list_[q].back();
      break;
    }
  }

  priority_to_stream_id_list_[priority].push_back(stream_id);

  if (protocol_ < kProtoHTTP2) {
    SpdySynStreamIR syn_stream(stream_id);
    syn_stream.set_header_block(block);
    syn_stream.set_priority(
        ConvertRequestPriorityToSpdyPriority(priority, spdy_version()));
    syn_stream.set_fin(fin);
    return CreateFramer(compressed)->SerializeFrame(syn_stream);
  } else {
    SpdyHeadersIR headers(stream_id);
    headers.set_header_block(block);
    headers.set_has_priority(true);
    headers.set_priority(
        ConvertRequestPriorityToSpdyPriority(priority, spdy_version()));
    if (dependency_priorities_) {
      headers.set_parent_stream_id(parent_stream_id);
      headers.set_exclusive(true);
    }
    headers.set_fin(fin);
    return CreateFramer(compressed)->SerializeFrame(headers);
  }
}

SpdyFrame* SpdyTestUtil::ConstructSpdyReply(int stream_id,
                                            const SpdyHeaderBlock& headers) {
  if (protocol_ < kProtoHTTP2) {
    SpdySynReplyIR syn_reply(stream_id);
    syn_reply.set_header_block(headers);
    return CreateFramer(false)->SerializeFrame(syn_reply);
  } else {
    SpdyHeadersIR reply(stream_id);
    reply.set_header_block(headers);
    return CreateFramer(false)->SerializeFrame(reply);
  }
}

SpdyFrame* SpdyTestUtil::ConstructSpdySynReplyError(
    const char* const status,
    const char* const* const extra_headers,
    int extra_header_count,
    int stream_id) {
  SpdyHeaderBlock block;
  block[GetStatusKey()] = status;
  MaybeAddVersionHeader(&block);
  block["hello"] = "bye";
  AppendToHeaderBlock(extra_headers, extra_header_count, &block);

  return ConstructSpdyReply(stream_id, block);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGetSynReplyRedirect(int stream_id) {
  static const char* const kExtraHeaders[] = {
    "location", "http://www.foo.com/index.php",
  };
  return ConstructSpdySynReplyError("301 Moved Permanently", kExtraHeaders,
                                    arraysize(kExtraHeaders)/2, stream_id);
}

SpdyFrame* SpdyTestUtil::ConstructSpdySynReplyError(int stream_id) {
  return ConstructSpdySynReplyError("500 Internal Server Error", NULL, 0, 1);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyGetSynReply(
    const char* const extra_headers[],
    int extra_header_count,
    int stream_id) {
  SpdyHeaderBlock block;
  block[GetStatusKey()] = "200";
  MaybeAddVersionHeader(&block);
  block["hello"] = "bye";
  AppendToHeaderBlock(extra_headers, extra_header_count, &block);

  return ConstructSpdyReply(stream_id, block);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPost(const char* url,
                                           SpdyStreamId stream_id,
                                           int64_t content_length,
                                           RequestPriority priority,
                                           const char* const extra_headers[],
                                           int extra_header_count) {
  scoped_ptr<SpdyHeaderBlock> block(
      ConstructPostHeaderBlock(url, content_length));
  AppendToHeaderBlock(extra_headers, extra_header_count, block.get());
  return ConstructSpdySyn(stream_id, *block, priority, false, false);
}

SpdyFrame* SpdyTestUtil::ConstructChunkedSpdyPost(
    const char* const extra_headers[],
    int extra_header_count) {
  SpdyHeaderBlock block;
  MaybeAddVersionHeader(&block);
  block[GetMethodKey()] = "POST";
  AddUrlToHeaderBlock(default_url_.spec(), &block);
  AppendToHeaderBlock(extra_headers, extra_header_count, &block);
  return ConstructSpdySyn(1, block, LOWEST, false, false);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyPostSynReply(
    const char* const extra_headers[],
    int extra_header_count) {
  // TODO(jgraettinger): Remove this method.
  return ConstructSpdyGetSynReply(extra_headers, extra_header_count, 1);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyBodyFrame(int stream_id, bool fin) {
  SpdyFramer framer(spdy_version_);
  SpdyDataIR data_ir(stream_id,
                     base::StringPiece(kUploadData, kUploadDataSize));
  data_ir.set_fin(fin);
  return framer.SerializeData(data_ir);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyBodyFrame(int stream_id,
                                                const char* data,
                                                uint32_t len,
                                                bool fin) {
  SpdyFramer framer(spdy_version_);
  SpdyDataIR data_ir(stream_id, base::StringPiece(data, len));
  data_ir.set_fin(fin);
  return framer.SerializeData(data_ir);
}

SpdyFrame* SpdyTestUtil::ConstructSpdyBodyFrame(int stream_id,
                                                const char* data,
                                                uint32_t len,
                                                bool fin,
                                                int padding_length) {
  SpdyFramer framer(spdy_version_);
  SpdyDataIR data_ir(stream_id, base::StringPiece(data, len));
  data_ir.set_fin(fin);
  data_ir.set_padding_len(padding_length);
  return framer.SerializeData(data_ir);
}

SpdyFrame* SpdyTestUtil::ConstructWrappedSpdyFrame(
    const scoped_ptr<SpdyFrame>& frame,
    int stream_id) {
  return ConstructSpdyBodyFrame(stream_id, frame->data(),
                                frame->size(), false);
}

void SpdyTestUtil::UpdateWithStreamDestruction(int stream_id) {
  for (auto priority_it = priority_to_stream_id_list_.begin();
       priority_it != priority_to_stream_id_list_.end(); ++priority_it) {
    for (auto stream_it = priority_it->second.begin();
         stream_it != priority_it->second.end(); ++stream_it) {
      if (*stream_it == stream_id) {
        priority_it->second.erase(stream_it);
        return;
      }
    }
  }
  NOTREACHED();
}

scoped_ptr<SpdyFramer> SpdyTestUtil::CreateFramer(bool compressed) const {
  scoped_ptr<SpdyFramer> framer(new SpdyFramer(spdy_version_));
  framer->set_enable_compression(compressed);
  return framer;
}

const char* SpdyTestUtil::GetMethodKey() const {
  return ":method";
}

const char* SpdyTestUtil::GetStatusKey() const {
  return ":status";
}

const char* SpdyTestUtil::GetHostKey() const {
  if (protocol_ < kProtoHTTP2)
    return ":host";
  else
    return ":authority";
}

const char* SpdyTestUtil::GetSchemeKey() const {
  return ":scheme";
}

const char* SpdyTestUtil::GetVersionKey() const {
  return ":version";
}

const char* SpdyTestUtil::GetPathKey() const {
  return ":path";
}

scoped_ptr<SpdyHeaderBlock> SpdyTestUtil::ConstructHeaderBlock(
    base::StringPiece method,
    base::StringPiece url,
    int64_t* content_length) const {
  std::string scheme, host, path;
  ParseUrl(url.data(), &scheme, &host, &path);
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  if (include_version_header()) {
    (*headers)[GetVersionKey()] = "HTTP/1.1";
  }
  (*headers)[GetMethodKey()] = method.as_string();
  (*headers)[GetHostKey()] = host.c_str();
  (*headers)[GetSchemeKey()] = scheme.c_str();
  (*headers)[GetPathKey()] = path.c_str();
  if (content_length) {
    std::string length_str = base::Int64ToString(*content_length);
    (*headers)["content-length"] = length_str;
  }
  return headers;
}

void SpdyTestUtil::MaybeAddVersionHeader(
    SpdyFrameWithHeaderBlockIR* frame_ir) const {
  if (include_version_header()) {
    frame_ir->SetHeader(GetVersionKey(), "HTTP/1.1");
  }
}

void SpdyTestUtil::MaybeAddVersionHeader(SpdyHeaderBlock* block) const {
  if (include_version_header()) {
    (*block)[GetVersionKey()] = "HTTP/1.1";
  }
}

void SpdyTestUtil::SetPriority(RequestPriority priority,
                               SpdySynStreamIR* ir) const {
  ir->set_priority(ConvertRequestPriorityToSpdyPriority(
      priority, spdy_version()));
}

}  // namespace net
