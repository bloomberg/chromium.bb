// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/network_quality_estimator.h"
#include "net/base/socket_performance_watcher.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_packet_maker.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {
namespace test {

namespace {

static const char kQuicAlternateProtocolHeader[] =
    "Alternate-Protocol: 443:quic\r\n\r\n";
static const char kQuicAlternateProtocol50pctHeader[] =
    "Alternate-Protocol: 443:quic,p=.5\r\n\r\n";
static const char kQuicAlternateProtocolDifferentPortHeader[] =
    "Alternate-Protocol: 137:quic\r\n\r\n";
static const char kQuicAlternativeServiceHeader[] =
    "Alt-Svc: quic=\":443\"\r\n\r\n";
static const char kQuicAlternativeService50pctHeader[] =
    "Alt-Svc: quic=\":443\";p=\".5\"\r\n\r\n";
static const char kQuicAlternativeServiceDifferentPortHeader[] =
    "Alt-Svc: quic=\":137\"\r\n\r\n";

const char kDefaultServerHostName[] = "mail.example.com";

}  // namespace

// Helper class to encapsulate MockReads and MockWrites for QUIC.
// Simplify ownership issues and the interaction with the MockSocketFactory.
class MockQuicData {
 public:
  MockQuicData() : packet_number_(0) {}

  ~MockQuicData() { STLDeleteElements(&packets_); }

  void AddSynchronousRead(scoped_ptr<QuicEncryptedPacket> packet) {
    reads_.push_back(MockRead(SYNCHRONOUS, packet->data(), packet->length(),
                              packet_number_++));
    packets_.push_back(packet.release());
  }

  void AddRead(scoped_ptr<QuicEncryptedPacket> packet) {
    reads_.push_back(
        MockRead(ASYNC, packet->data(), packet->length(), packet_number_++));
    packets_.push_back(packet.release());
  }

  void AddRead(IoMode mode, int rv) {
    reads_.push_back(MockRead(mode, rv, packet_number_++));
  }

  void AddWrite(scoped_ptr<QuicEncryptedPacket> packet) {
    writes_.push_back(MockWrite(SYNCHRONOUS, packet->data(), packet->length(),
                                packet_number_++));
    packets_.push_back(packet.release());
  }

  void AddSocketDataToFactory(MockClientSocketFactory* factory) {
    MockRead* reads = reads_.empty() ? nullptr : &reads_[0];
    MockWrite* writes = writes_.empty() ? nullptr : &writes_[0];
    socket_data_.reset(
        new SequencedSocketData(reads, reads_.size(), writes, writes_.size()));
    factory->AddSocketDataProvider(socket_data_.get());
  }

  void Resume() { socket_data_->Resume(); }

 private:
  std::vector<QuicEncryptedPacket*> packets_;
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  size_t packet_number_;
  scoped_ptr<SequencedSocketData> socket_data_;
};

class ProxyHeadersHandler {
 public:
  ProxyHeadersHandler() : was_called_(false) {}

  bool was_called() { return was_called_; }

  void OnBeforeProxyHeadersSent(const ProxyInfo& proxy_info,
                                HttpRequestHeaders* request_headers) {
    was_called_ = true;
  }

 private:
  bool was_called_;
};

class TestNetworkQualityEstimator : public NetworkQualityEstimator {
 public:
  TestNetworkQualityEstimator()
      : NetworkQualityEstimator(scoped_ptr<net::ExternalEstimateProvider>(),
                                std::map<std::string, std::string>()),
        watcher_count_(0) {}

  ~TestNetworkQualityEstimator() override {}

  scoped_ptr<SocketPerformanceWatcher> CreateSocketPerformanceWatcher(
      const Protocol protocol) override {
    if (protocol != PROTOCOL_QUIC) {
      NOTIMPLEMENTED();
    }
    ++watcher_count_;
    return NetworkQualityEstimator::CreateSocketPerformanceWatcher(protocol);
  }

  size_t watcher_count() const { return watcher_count_; }

 private:
  size_t watcher_count_;
  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityEstimator);
};

class TestRTTObserver : public NetworkQualityEstimator::RTTObserver {
 public:
  TestRTTObserver() : rtt_notification_received_(false) {}

  bool rtt_notification_received() const { return rtt_notification_received_; }

  // NetworkQualityEstimator::RttObserver implementation:
  void OnRTTObservation(
      int32_t rtt_ms,
      const base::TimeTicks& timestamp,
      net::NetworkQualityEstimator::ObservationSource source) override {
    rtt_notification_received_ = true;
  }

 private:
  bool rtt_notification_received_;
};

class QuicNetworkTransactionTest
    : public PlatformTest,
      public ::testing::WithParamInterface<QuicVersion> {
 protected:
  QuicNetworkTransactionTest()
      : clock_(new MockClock),
        maker_(GetParam(), 0, clock_, kDefaultServerHostName),
        cert_transparency_verifier_(new MultiLogCTVerifier()),
        test_network_quality_estimator_(new TestNetworkQualityEstimator()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        proxy_service_(ProxyService::CreateDirect()),
        auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        random_generator_(0),
        ssl_data_(ASYNC, OK) {
    request_.method = "GET";
    std::string url("https://");
    url.append(kDefaultServerHostName);
    request_.url = GURL(url);
    request_.load_flags = 0;
    clock_->AdvanceTime(QuicTime::Delta::FromMilliseconds(20));

    scoped_refptr<X509Certificate> cert(
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
    verify_details_.cert_verify_result.verified_cert = cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);
  }

  void SetUp() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
  }

  void TearDown() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    base::MessageLoop::current()->RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
  }

  scoped_ptr<QuicEncryptedPacket> ConstructConnectionClosePacket(
      QuicPacketNumber num) {
    return maker_.MakeConnectionClosePacket(num);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckPacket(
      QuicPacketNumber largest_received,
      QuicPacketNumber least_unacked) {
    return maker_.MakeAckPacket(2, largest_received, least_unacked,
                                least_unacked, true);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckPacket(
      QuicPacketNumber largest_received,
      QuicPacketNumber least_unacked,
      QuicTestPacketMaker* maker) {
    return maker->MakeAckPacket(2, largest_received, least_unacked,
                                least_unacked, true);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckAndConnectionClosePacket(
      QuicPacketNumber packet_number,
      QuicPacketNumber largest_received,
      QuicPacketNumber ack_least_unacked,
      QuicPacketNumber stop_least_unacked) {
    return maker_.MakeAckPacket(packet_number, largest_received,
                                ack_least_unacked, stop_least_unacked, true);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckAndConnectionClosePacket(
      QuicPacketNumber num,
      QuicTime::Delta delta_time_largest_observed,
      QuicPacketNumber largest_received,
      QuicPacketNumber least_unacked,
      QuicErrorCode quic_error,
      std::string& quic_error_details) {
    return maker_.MakeAckAndConnectionClosePacket(
        num, false, delta_time_largest_observed, largest_received,
        least_unacked, quic_error, quic_error_details);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstPacket(
      QuicPacketNumber num,
      bool include_version,
      QuicStreamId stream_id,
      QuicRstStreamErrorCode error_code) {
    return maker_.MakeRstPacket(num, include_version, stream_id, error_code);
  }

  // Uses default QuicTestPacketMaker.
  SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                    const std::string& scheme,
                                    const std::string& path) {
    return GetRequestHeaders(method, scheme, path, maker_);
  }

  // Uses customized QuicTestPacketMaker.
  SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                    const std::string& scheme,
                                    const std::string& path,
                                    QuicTestPacketMaker& maker) {
    return maker.GetRequestHeaders(method, scheme, path);
  }

  SpdyHeaderBlock GetResponseHeaders(const std::string& status) {
    return maker_.GetResponseHeaders(status);
  }

  // Appends alt_svc headers in the response headers.
  SpdyHeaderBlock GetResponseHeaders(const std::string& status,
                                     const std::string& alt_svc) {
    return maker_.GetResponseHeaders(status, alt_svc);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructDataPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      base::StringPiece data) {
    return maker_.MakeDataPacket(packet_number, stream_id,
                                 should_include_version, fin, offset, data);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRequestHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers,
      QuicStreamOffset* offset) {
    SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    return maker_.MakeRequestHeadersPacketWithOffsetTracking(
        packet_number, stream_id, should_include_version, fin, priority,
        headers, offset);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRequestHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers,
      QuicStreamOffset* offset,
      QuicTestPacketMaker* maker) {
    SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    return maker->MakeRequestHeadersPacketWithOffsetTracking(
        packet_number, stream_id, should_include_version, fin, priority,
        headers, offset);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRequestHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers) {
    return ConstructRequestHeadersPacket(packet_number, stream_id,
                                         should_include_version, fin, headers,
                                         nullptr, &maker_);
  }
  scoped_ptr<QuicEncryptedPacket> ConstructRequestHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers,
      QuicTestPacketMaker* maker) {
    return ConstructRequestHeadersPacket(packet_number, stream_id,
                                         should_include_version, fin, headers,
                                         nullptr, maker);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructResponseHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers) {
    return ConstructResponseHeadersPacket(packet_number, stream_id,
                                          should_include_version, fin, headers,
                                          nullptr, &maker_);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructResponseHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers,
      QuicTestPacketMaker* maker) {
    return ConstructResponseHeadersPacket(packet_number, stream_id,
                                          should_include_version, fin, headers,
                                          nullptr, maker);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructResponseHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers,
      QuicStreamOffset* offset) {
    return maker_.MakeResponseHeadersPacketWithOffsetTracking(
        packet_number, stream_id, should_include_version, fin, headers, offset);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructResponseHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers,
      QuicStreamOffset* offset,
      QuicTestPacketMaker* maker) {
    return maker->MakeResponseHeadersPacketWithOffsetTracking(
        packet_number, stream_id, should_include_version, fin, headers, offset);
  }

  void CreateSession() { CreateSessionWithFactory(&socket_factory_, false); }

  void CreateSessionWithNextProtos() {
    CreateSessionWithFactory(&socket_factory_, true);
  }

  // If |use_next_protos| is true, enables SPDY and QUIC.
  void CreateSessionWithFactory(ClientSocketFactory* socket_factory,
                                bool use_next_protos) {
    params_.enable_quic = true;
    params_.quic_clock = clock_;
    params_.quic_random = &random_generator_;
    params_.client_socket_factory = socket_factory;
    params_.quic_crypto_client_stream_factory = &crypto_client_stream_factory_;
    params_.host_resolver = &host_resolver_;
    params_.cert_verifier = &cert_verifier_;
    params_.transport_security_state = &transport_security_state_;
    params_.cert_transparency_verifier = cert_transparency_verifier_.get();
    params_.socket_performance_watcher_factory =
        test_network_quality_estimator_.get();
    params_.proxy_service = proxy_service_.get();
    params_.ssl_config_service = ssl_config_service_.get();
    params_.http_auth_handler_factory = auth_handler_factory_.get();
    params_.http_server_properties = http_server_properties_.GetWeakPtr();
    params_.quic_supported_versions = SupportedVersions(GetParam());
    for (const char* host :
         {kDefaultServerHostName, "www.example.com", "news.example.com",
          "bar.example.com", "foo.example.com", "www.example.org",
          "invalid.example.org", "docs.example.org"}) {
      params_.quic_host_whitelist.insert(host);
    }

    test_network_quality_estimator_->AddRTTObserver(&rtt_observer_);

    if (use_next_protos) {
      params_.use_alternative_services = true;
      params_.next_protos = NextProtosWithSpdyAndQuic(true, true);
    }

    session_.reset(new HttpNetworkSession(params_));
    session_->quic_stream_factory()->set_require_confirmation(false);
    ASSERT_EQ(params_.quic_socket_receive_buffer_size,
              session_->quic_stream_factory()->socket_receive_buffer_size());
  }

  void CheckWasQuicResponse(const scoped_ptr<HttpNetworkTransaction>& trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_npn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_QUIC1_SPDY3,
              response->connection_info);
  }

  void CheckResponsePort(const scoped_ptr<HttpNetworkTransaction>& trans,
                         uint16_t port) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(port, response->socket_address.port());
  }

  void CheckWasHttpResponse(const scoped_ptr<HttpNetworkTransaction>& trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_FALSE(response->was_fetched_via_spdy);
    EXPECT_FALSE(response->was_npn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_HTTP1,
              response->connection_info);
  }

  void CheckResponseData(const scoped_ptr<HttpNetworkTransaction>& trans,
                         const std::string& expected) {
    std::string response_data;
    ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
    EXPECT_EQ(expected, response_data);
  }

  void RunTransaction(const scoped_ptr<HttpNetworkTransaction>& trans) {
    TestCompletionCallback callback;
    int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_EQ(OK, callback.WaitForResult());
  }

  void SendRequestAndExpectHttpResponse(const std::string& expected) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    RunTransaction(trans);
    CheckWasHttpResponse(trans);
    CheckResponseData(trans, expected);
  }

  void SendRequestAndExpectQuicResponse(const std::string& expected) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, false, 443);
  }

  void SendRequestAndExpectQuicResponseOnPort(const std::string& expected,
                                              uint16_t port) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, false, port);
  }

  void SendRequestAndExpectQuicResponseFromProxyOnPort(
      const std::string& expected,
      uint16_t port) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, true, port);
  }

  void AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::HandshakeMode handshake_mode) {
    crypto_client_stream_factory_.set_handshake_mode(handshake_mode);
    HostPortPair host_port_pair = HostPortPair::FromURL(request_.url);
    AlternativeService alternative_service(QUIC, host_port_pair.host(), 443);
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    http_server_properties_.SetAlternativeService(
        host_port_pair, alternative_service, 1.0, expiration);
  }

  void ExpectBrokenAlternateProtocolMapping() {
    const HostPortPair origin = HostPortPair::FromURL(request_.url);
    const AlternativeServiceVector alternative_service_vector =
        http_server_properties_.GetAlternativeServices(origin);
    EXPECT_EQ(1u, alternative_service_vector.size());
    EXPECT_TRUE(http_server_properties_.IsAlternativeServiceBroken(
        alternative_service_vector[0]));
  }

  void ExpectQuicAlternateProtocolMapping() {
    const HostPortPair origin = HostPortPair::FromURL(request_.url);
    const AlternativeServiceVector alternative_service_vector =
        http_server_properties_.GetAlternativeServices(origin);
    EXPECT_EQ(1u, alternative_service_vector.size());
    EXPECT_EQ(QUIC, alternative_service_vector[0].protocol);
  }

  void AddHangingNonAlternateProtocolSocketData() {
    scoped_ptr<StaticSocketDataProvider> hanging_data;
    hanging_data.reset(new StaticSocketDataProvider());
    MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
    hanging_data->set_connect_data(hanging_connect);
    hanging_data_.push_back(std::move(hanging_data));
    socket_factory_.AddSocketDataProvider(hanging_data_.back().get());
    socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  }

  MockClock* clock_;  // Owned by QuicStreamFactory after CreateSession.
  QuicTestPacketMaker maker_;
  scoped_ptr<HttpNetworkSession> session_;
  MockClientSocketFactory socket_factory_;
  ProofVerifyDetailsChromium verify_details_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  scoped_ptr<CTVerifier> cert_transparency_verifier_;
  scoped_ptr<TestNetworkQualityEstimator> test_network_quality_estimator_;
  TestRTTObserver rtt_observer_;
  scoped_refptr<SSLConfigServiceDefaults> ssl_config_service_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  MockRandom random_generator_;
  HttpServerPropertiesImpl http_server_properties_;
  HttpNetworkSession::Params params_;
  HttpRequestInfo request_;
  BoundTestNetLog net_log_;
  std::vector<scoped_ptr<StaticSocketDataProvider>> hanging_data_;
  SSLSocketDataProvider ssl_data_;

 private:
  void SendRequestAndExpectQuicResponseMaybeFromProxy(
      const std::string& expected,
      bool used_proxy,
      uint16_t port) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    ProxyHeadersHandler proxy_headers_handler;
    trans->SetBeforeProxyHeadersSentCallback(
        base::Bind(&ProxyHeadersHandler::OnBeforeProxyHeadersSent,
                   base::Unretained(&proxy_headers_handler)));
    RunTransaction(trans);
    CheckWasQuicResponse(trans);
    CheckResponsePort(trans, port);
    CheckResponseData(trans, expected);
    EXPECT_EQ(used_proxy, proxy_headers_handler.was_called());
  }
};

INSTANTIATE_TEST_CASE_P(Version,
                        QuicNetworkTransactionTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicNetworkTransactionTest, ForceQuic) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("mail.example.com:443");

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  EXPECT_FALSE(rtt_observer_.rtt_notification_received());
  SendRequestAndExpectQuicResponse("hello!");
  EXPECT_TRUE(rtt_observer_.rtt_notification_received());

  // Check that the NetLog was filled reasonably.
  TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged a QUIC_SESSION_PACKET_RECEIVED.
  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_QUIC_SESSION_PACKET_RECEIVED,
      NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  // ... and also a TYPE_QUIC_SESSION_UNAUTHENTICATED_PACKET_HEADER_RECEIVED.
  pos = ExpectLogContainsSomewhere(
      entries, 0,
      NetLog::TYPE_QUIC_SESSION_UNAUTHENTICATED_PACKET_HEADER_RECEIVED,
      NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  std::string packet_number;
  ASSERT_TRUE(entries[pos].GetStringValue("packet_number", &packet_number));
  EXPECT_EQ("1", packet_number);

  // ... and also a TYPE_QUIC_SESSION_PACKET_AUTHENTICATED.
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_QUIC_SESSION_PACKET_AUTHENTICATED,
      NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  // ... and also a QUIC_SESSION_STREAM_FRAME_RECEIVED.
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_QUIC_SESSION_STREAM_FRAME_RECEIVED,
      NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  int log_stream_id;
  ASSERT_TRUE(entries[pos].GetIntegerValue("stream_id", &log_stream_id));
  EXPECT_EQ(3, log_stream_id);
}

TEST_P(QuicNetworkTransactionTest, QuicProxy) {
  params_.enable_quic_for_proxies = true;
  proxy_service_ =
      ProxyService::CreateFixedFromPacResult("QUIC mail.example.com:70");

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  EXPECT_FALSE(rtt_observer_.rtt_notification_received());
  // There is no need to set up an alternate protocol job, because
  // no attempt will be made to speak to the proxy over TCP.

  request_.url = GURL("http://mail.example.com/");
  CreateSession();

  SendRequestAndExpectQuicResponseFromProxyOnPort("hello!", 70);
  EXPECT_TRUE(rtt_observer_.rtt_notification_received());
}

// Regression test for https://crbug.com/492458.  Test that for an HTTP
// connection through a QUIC proxy, the certificate exhibited by the proxy is
// checked against the proxy hostname, not the origin hostname.
TEST_P(QuicNetworkTransactionTest, QuicProxyWithCert) {
  const std::string origin_host = "news.example.com";
  const std::string proxy_host = "www.example.org";

  params_.enable_quic_for_proxies = true;
  proxy_service_ =
      ProxyService::CreateFixedFromPacResult("QUIC " + proxy_host + ":70");

  maker_.set_hostname(origin_host);
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
  ASSERT_TRUE(cert.get());
  // This certificate is valid for the proxy, but not for the origin.
  bool common_name_fallback_used;
  EXPECT_TRUE(cert->VerifyNameMatch(proxy_host, &common_name_fallback_used));
  EXPECT_FALSE(cert->VerifyNameMatch(origin_host, &common_name_fallback_used));
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  request_.url = GURL("http://" + origin_host);
  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectQuicResponseFromProxyOnPort("hello!", 70);
}

TEST_P(QuicNetworkTransactionTest, ForceQuicWithErrorConnecting) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("mail.example.com:443");

  MockQuicData mock_quic_data1;
  mock_quic_data1.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);

  MockQuicData mock_quic_data2;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);
  mock_quic_data2.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);

  mock_quic_data1.AddSocketDataToFactory(&socket_factory_);
  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  EXPECT_EQ(0U, test_network_quality_estimator_->watcher_count());
  for (size_t i = 0; i < 2; ++i) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    TestCompletionCallback callback;
    int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_EQ(ERR_CONNECTION_CLOSED, callback.WaitForResult());
    EXPECT_EQ(1 + i, test_network_quality_estimator_->watcher_count());
  }
}

TEST_P(QuicNetworkTransactionTest, DoNotForceQuicForHttps) {
  // Attempt to "force" quic on 443, which will not be honored.
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:443");

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider data(http_reads, arraysize(http_reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  EXPECT_EQ(0U, test_network_quality_estimator_->watcher_count());
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceQuicSupportedVersion) {
  std::string altsvc_header = base::StringPrintf(
      "Alt-Svc: quic=\":443\"; v=\"%u\"\r\n\r\n", GetParam());
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(altsvc_header.c_str()),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest,
       DoNotUseAlternativeServiceQuicUnsupportedVersion) {
  std::string altsvc_header = base::StringPrintf(
      "Alt-Svc: quic=\":443\"; v=\"%u\"\r\n\r\n", GetParam() - 1);
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(altsvc_header.c_str()),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

// When multiple alternative services are advertised,
// HttpStreamFactoryImpl::RequestStreamInternal() should select the alternative
// service which uses existing QUIC session if available. If no existing QUIC
// session can be used, use the first alternative service from the list.
TEST_P(QuicNetworkTransactionTest, UseExistingAlternativeServiceForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\"foo.example.com:443\", quic=\":444\"\r\n\r\n"),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  QuicStreamOffset request_header_offset = 0;
  QuicStreamOffset response_header_offset = 0;
  // First QUIC request data.
  // Open a session to foo.example.com:443 using the first entry of the
  // alternative service list.
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(ConstructRequestHeadersPacket(
      1, kClientDataStreamId1, true, true,
      GetRequestHeaders("GET", "https", "/"), &request_header_offset));

  std::string alt_svc_list =
      "quic=\"mail.example.com:444\", quic=\"foo.example.com:443\", "
      "quic=\"bar.example.com:445\"";
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false,
      GetResponseHeaders("200 OK", alt_svc_list), &response_header_offset));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));

  // Second QUIC request data.
  // Connection pooling, using existing session, no need to include version
  // as version negotiation has been completed.
  mock_quic_data.AddWrite(ConstructRequestHeadersPacket(
      3, kClientDataStreamId2, false, true,
      GetRequestHeaders("GET", "https", "/"), &request_header_offset));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      3, kClientDataStreamId2, false, false, GetResponseHeaders("200 OK"),
      &response_header_offset));
  mock_quic_data.AddRead(
      ConstructDataPacket(4, kClientDataStreamId2, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckAndConnectionClosePacket(4, 4, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");

  SendRequestAndExpectQuicResponseOnPort("hello!", 443);
  SendRequestAndExpectQuicResponseOnPort("hello!", 443);
}

// When multiple alternative services that has existing QUIC session.
// HttpStreamFactoryImpl::RequestStreamInternal() should select the first
// alternative service which uses existing QUIC session.
TEST_P(QuicNetworkTransactionTest, UseFirstExistingAlternativeServiceForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\"foo.example.com:443\", quic=\":446\"\r\n\r\n"),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  QuicStreamOffset request_header_offset = 0;
  QuicStreamOffset response_header_offset = 0;

  QuicTestPacketMaker maker(GetParam(), 0, clock_, kDefaultServerHostName);

  MockQuicData mock_quic_data;
  MockQuicData mock_quic_data2;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);
  // First QUIC request data.
  // Open a QUIC session to foo.example.com:443.
  mock_quic_data.AddWrite(ConstructRequestHeadersPacket(
      1, kClientDataStreamId1, true, true,
      GetRequestHeaders("GET", "https", "/"), &request_header_offset));

  std::string alt_svc_list =
      "quic=\"bar.example.com:444\", quic=\"frog.example.com:445\", "
      "quic=\"mail.example.com:446\"";
  // Response header from the server resets the alt_svc list for the origin.
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false,
      GetResponseHeaders("200 OK", alt_svc_list), &response_header_offset));
  mock_quic_data.AddRead(ConstructDataPacket(2, kClientDataStreamId1, false,
                                             true, 0, "hello from foo!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));

  // Second QUIC request data.
  // Existing QUIC session to foo.example.com is not viable from the updated
  // alt_svc. Unable to pool the existing QUIC session.
  // Open a new QUIC session to bar.example.com:443.
  mock_quic_data2.AddWrite(ConstructRequestHeadersPacket(
      1, kClientDataStreamId1, true, true,
      GetRequestHeaders("GET", "https", "/"), &maker));
  alt_svc_list =
      "quic=\"foo.example.com:443\", quic=\"mail.example.com:446\", "
      "quic=\"bar.example.com:444\"";
  // Response header from the server resets the alt_svc list for the origin.
  mock_quic_data2.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false,
      GetResponseHeaders("200 OK", alt_svc_list), &maker));
  mock_quic_data2.AddRead(ConstructDataPacket(2, kClientDataStreamId1, false,
                                              true, 0, "hello from bar!"));
  mock_quic_data2.AddWrite(ConstructAckPacket(2, 1, &maker));
  mock_quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data2.AddRead(ASYNC, 0);               // EOF

  // Third QUIC request data.
  // Connection pooling, using the first existing session to foo.example.com
  mock_quic_data.AddWrite(ConstructRequestHeadersPacket(
      3, kClientDataStreamId2, false, true,
      GetRequestHeaders("GET", "https", "/"), &request_header_offset));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      3, kClientDataStreamId2, false, false, GetResponseHeaders("200 OK"),
      &response_header_offset));
  mock_quic_data.AddRead(ConstructDataPacket(4, kClientDataStreamId2, false,
                                             true, 0, "hello from foo!"));
  mock_quic_data.AddWrite(ConstructAckAndConnectionClosePacket(4, 4, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  AddHangingNonAlternateProtocolSocketData();

  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponseOnPort("hello from foo!", 443);
  SendRequestAndExpectQuicResponseOnPort("hello from bar!", 444);
  SendRequestAndExpectQuicResponseOnPort("hello from foo!", 443);
}

// Multiple origins have listed the same alternative services. When there's a
// existing QUIC session opened by a request to other origin,
// if the cert is valid, should select this QUIC session to make the request
// if this is also the first existing QUIC session.
TEST_P(QuicNetworkTransactionTest,
       UseSharedExistingAlternativeServiceForQuicWithValidCert) {
  // Default cert is valid for the following origins:
  // mail.example.com, mail.example.org, and www.example.org.

  // HTTP data for request to mail.example.com.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\":443\"\r\n\r\n"),
      MockRead("hello world from mail.example.com"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // HTTP data for request to mail.example.org.
  MockRead http_reads2[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\":444\", quic=\"mail.example.com:443\"\r\n\r\n"),
      MockRead("hello world from mail.example.org"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data2(http_reads2, arraysize(http_reads2),
                                      nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data2);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  QuicStreamOffset request_header_offset = 0;
  QuicStreamOffset response_header_offset = 0;

  QuicTestPacketMaker maker(GetParam(), 0, clock_, kDefaultServerHostName);
  maker.set_hostname("mail.example.org");
  MockQuicData mock_quic_data;

  // First QUIC request data.
  mock_quic_data.AddWrite(ConstructRequestHeadersPacket(
      1, kClientDataStreamId1, true, true,
      GetRequestHeaders("GET", "https", "/"), &request_header_offset));

  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK"),
      &response_header_offset));
  mock_quic_data.AddRead(ConstructDataPacket(2, kClientDataStreamId1, false,
                                             true, 0, "hello from mail QUIC!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));

  // Second QUIC request data.
  mock_quic_data.AddWrite(ConstructRequestHeadersPacket(
      3, kClientDataStreamId2, false, true,
      GetRequestHeaders("GET", "https", "/", maker), &request_header_offset,
      &maker));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      3, kClientDataStreamId2, false, false, GetResponseHeaders("200 OK"),
      &response_header_offset));
  mock_quic_data.AddRead(ConstructDataPacket(4, kClientDataStreamId2, false,
                                             true, 0, "hello from mail QUIC!"));
  mock_quic_data.AddWrite(ConstructAckAndConnectionClosePacket(4, 4, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  // Send two HTTP requests, responses set up alt-svc lists for the origins.
  SendRequestAndExpectHttpResponse("hello world from mail.example.com");
  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectHttpResponse("hello world from mail.example.org");

  // Open a QUIC session to mail.example.com:443 when making request
  // to mail.example.com.
  request_.url = GURL("https://mail.example.com/");
  SendRequestAndExpectQuicResponseOnPort("hello from mail QUIC!", 443);

  // Uses the existing QUIC session when making request to mail.example.org.
  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectQuicResponseOnPort("hello from mail QUIC!", 443);
}

// Multiple origins have listed the same alternative services. When there's a
// existing QUIC session opened by a request to other origin,
// if the cert is NOT valid, should ignore this QUIC session.
TEST_P(QuicNetworkTransactionTest,
       DoNotUseSharedExistingAlternativeServiceForQuicWithInvalidCert) {
  // Default cert is only valid for the following origins:
  // mail.example.com, mail.example.org, and www.example.org.
  // NOT valid for docs.example.org.

  // HTTP data for request to mail.example.com.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\":443\"\r\n\r\n"),
      MockRead("hello world from mail.example.com"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // HTTP data for request to docs.example.org.
  MockRead http_reads2[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\":444\", quic=\"mail.example.com:443\"\r\n\r\n"),
      MockRead("hello world from docs.example.org"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data2(http_reads2, arraysize(http_reads2),
                                      nullptr, 0);
  socket_factory_.AddSocketDataProvider(&http_data2);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  QuicTestPacketMaker maker(GetParam(), 0, clock_, kDefaultServerHostName);
  maker.set_hostname("docs.example.org");
  MockQuicData mock_quic_data;
  MockQuicData mock_quic_data2;

  // Adding a valid cert for *.example.org but not mail.example.com.
  ProofVerifyDetailsChromium verify_details;
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // First QUIC request data.
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(ConstructDataPacket(2, kClientDataStreamId1, false,
                                             true, 0, "hello from mail QUIC!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  // First QUIC request data.
  mock_quic_data2.AddWrite(ConstructRequestHeadersPacket(
      1, kClientDataStreamId1, true, true,
      GetRequestHeaders("GET", "https", "/", maker), &maker));
  mock_quic_data2.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK"), &maker));
  mock_quic_data2.AddRead(ConstructDataPacket(
      2, kClientDataStreamId1, false, true, 0, "hello from docs QUIC!"));
  mock_quic_data2.AddWrite(ConstructAckPacket(2, 1, &maker));
  mock_quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data2.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  AddHangingNonAlternateProtocolSocketData();

  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  // Send HTTP requests, responses set up the alt-svc lists for the origins.
  SendRequestAndExpectHttpResponse("hello world from mail.example.com");
  request_.url = GURL("https://docs.example.org/");
  SendRequestAndExpectHttpResponse("hello world from docs.example.org");

  // Open a QUIC session to mail.example.com:443 when making request
  // to mail.example.com.
  request_.url = GURL("https://mail.example.com/");
  SendRequestAndExpectQuicResponseOnPort("hello from mail QUIC!", 443);

  // Open another new QUIC session to docs.example.org:444.
  request_.url = GURL("https://docs.example.org/");
  SendRequestAndExpectQuicResponseOnPort("hello from docs QUIC!", 444);
}

TEST_P(QuicNetworkTransactionTest, AlternativeServiceDifferentPort) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternativeServiceDifferentPortHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponseOnPort("hello!", 137);
}

TEST_P(QuicNetworkTransactionTest, ConfirmAlternativeService) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();

  AlternativeService alternative_service(QUIC,
                                         HostPortPair::FromURL(request_.url));
  http_server_properties_.MarkAlternativeServiceRecentlyBroken(
      alternative_service);
  EXPECT_TRUE(http_server_properties_.WasAlternativeServiceRecentlyBroken(
      alternative_service));

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");

  mock_quic_data.Resume();

  EXPECT_FALSE(http_server_properties_.WasAlternativeServiceRecentlyBroken(
      alternative_service));
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceProbabilityForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternativeService50pctHeader), MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  params_.alternative_service_probability_threshold = 0.25;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest,
       DontUseAlternativeServiceProbabilityForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternativeService50pctHeader), MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  params_.alternative_service_probability_threshold = 0.75;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest,
       DontUseAlternativeServiceWithBadProbabilityForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\":443\";p=2\r\n\r\n"), MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  params_.alternative_service_probability_threshold = 0.75;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceForQuicForHttps) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();

  // TODO(rtenneti): Test QUIC over HTTPS, GetSSLInfo().
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, UseAlternateProtocolForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternateProtocolHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, AlternateProtocolDifferentPort) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternateProtocolDifferentPortHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponseOnPort("hello!", 137);
}

TEST_P(QuicNetworkTransactionTest, ConfirmAlternateProtocol) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternateProtocolHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  AlternativeService alternative_service(QUIC,
                                         HostPortPair::FromURL(request_.url));
  http_server_properties_.MarkAlternativeServiceRecentlyBroken(
      alternative_service);
  EXPECT_TRUE(http_server_properties_.WasAlternativeServiceRecentlyBroken(
      alternative_service));

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");

  mock_quic_data.Resume();

  EXPECT_FALSE(http_server_properties_.WasAlternativeServiceRecentlyBroken(
      alternative_service));
}

TEST_P(QuicNetworkTransactionTest, UseAlternateProtocolProbabilityForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternateProtocol50pctHeader), MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  params_.alternative_service_probability_threshold = .25;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, DontUseAlternateProtocolProbabilityForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternateProtocol50pctHeader), MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  params_.alternative_service_probability_threshold = .75;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest,
       DontUseAlternateProtocolWithBadProbabilityForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alternate-Protocol: 443:quic,p=2\r\n\r\n"),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  params_.alternative_service_probability_threshold = .75;
  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, UseAlternateProtocolForQuicForHttps) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternateProtocolHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  // TODO(rtenneti): Test QUIC over HTTPS, GetSSLInfo().
  SendRequestAndExpectHttpResponse("hello world");
}

class QuicAltSvcCertificateVerificationTest
    : public QuicNetworkTransactionTest {
 public:
  void Run(bool valid) {
    HostPortPair origin(valid ? "mail.example.org" : "invalid.example.org",
                        443);
    HostPortPair alternative("www.example.org", 443);
    std::string url("https://");
    url.append(origin.host());
    url.append(":443");
    request_.url = GURL(url);

    maker_.set_hostname(origin.host());
    MockQuicData mock_quic_data;
    mock_quic_data.AddWrite(
        ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                      GetRequestHeaders("GET", "https", "/")));
    mock_quic_data.AddRead(ConstructResponseHeadersPacket(
        1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
    mock_quic_data.AddRead(
        ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
    mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
    mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    mock_quic_data.AddSocketDataToFactory(&socket_factory_);

    scoped_refptr<X509Certificate> cert(
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
    ASSERT_TRUE(cert.get());
    bool common_name_fallback_used;
    EXPECT_EQ(valid,
              cert->VerifyNameMatch(origin.host(), &common_name_fallback_used));
    EXPECT_TRUE(
        cert->VerifyNameMatch(alternative.host(), &common_name_fallback_used));
    ProofVerifyDetailsChromium verify_details;
    verify_details.cert_verify_result.verified_cert = cert;
    verify_details.cert_verify_result.is_issued_by_known_root = true;
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
    crypto_client_stream_factory_.set_handshake_mode(
        MockCryptoClientStream::CONFIRM_HANDSHAKE);

    // Connection to |origin| fails, so that success of |request| depends on
    // connection to |alternate| only.
    MockConnect refused_connect(ASYNC, ERR_CONNECTION_REFUSED);
    StaticSocketDataProvider refused_data;
    refused_data.set_connect_data(refused_connect);
    socket_factory_.AddSocketDataProvider(&refused_data);

    CreateSessionWithNextProtos();
    AlternativeService alternative_service(QUIC, alternative);
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    session_->http_server_properties()->SetAlternativeService(
        origin, alternative_service, 1.0, expiration);
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    TestCompletionCallback callback;
    int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback.WaitForResult();
    if (valid) {
      EXPECT_EQ(OK, rv);
      CheckWasQuicResponse(trans);
      CheckResponsePort(trans, 443);
      CheckResponseData(trans, "hello!");
    } else {
      EXPECT_EQ(ERR_CONNECTION_REFUSED, rv);
    }
  }
};

INSTANTIATE_TEST_CASE_P(Version,
                        QuicAltSvcCertificateVerificationTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicAltSvcCertificateVerificationTest,
       RequestSucceedsWithValidCertificate) {
  Run(true);
}

TEST_P(QuicAltSvcCertificateVerificationTest,
       RequestFailsWithInvalidCertificate) {
  Run(false);
}

TEST_P(QuicNetworkTransactionTest, HungAlternateProtocol) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.com\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternateProtocolHeader),
      MockRead(SYNCHRONOUS, 5, "hello world"), MockRead(SYNCHRONOUS, OK, 6)};

  MockClientSocketFactory socket_factory;

  SequencedSocketData http_data(http_reads, arraysize(http_reads), http_writes,
                                arraysize(http_writes));
  socket_factory.AddSocketDataProvider(&http_data);
  socket_factory.AddSSLSocketDataProvider(&ssl_data_);

  // The QUIC transaction will not be allowed to complete.
  MockWrite quic_writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  MockRead quic_reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };
  SequencedSocketData quic_data(quic_reads, arraysize(quic_reads), quic_writes,
                                arraysize(quic_writes));
  socket_factory.AddSocketDataProvider(&quic_data);

  // The HTTP transaction will complete.
  SequencedSocketData http_data2(http_reads, arraysize(http_reads), http_writes,
                                 arraysize(http_writes));
  socket_factory.AddSocketDataProvider(&http_data2);
  socket_factory.AddSSLSocketDataProvider(&ssl_data_);

  CreateSessionWithFactory(&socket_factory, true);

  // Run the first request.
  SendRequestAndExpectHttpResponse("hello world");
  ASSERT_TRUE(http_data.AllReadDataConsumed());
  ASSERT_TRUE(http_data.AllWriteDataConsumed());

  // Now run the second request in which the QUIC socket hangs,
  // and verify the the transaction continues over HTTP.
  SendRequestAndExpectHttpResponse("hello world");
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(http_data2.AllReadDataConsumed());
  ASSERT_TRUE(http_data2.AllWriteDataConsumed());
  ASSERT_TRUE(quic_data.AllReadDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithHttpRace) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithNoHttpRace) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.com", "192.168.0.1",
                                           "");
  HostResolver::RequestInfo info(HostPortPair("mail.example.com", 443));
  AddressList address;
  host_resolver_.Resolve(info, DEFAULT_PRIORITY, &address, CompletionCallback(),
                         nullptr, net_log_.bound());

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithProxy) {
  proxy_service_ = ProxyService::CreateFixedFromPacResult("PROXY myproxy:70");

  // Since we are using a proxy, the QUIC job will not succeed.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET http://mail.example.com/ HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.com\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Proxy-Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternateProtocolHeader),
      MockRead(SYNCHRONOUS, 5, "hello world"), MockRead(SYNCHRONOUS, OK, 6)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     http_writes, arraysize(http_writes));
  socket_factory_.AddSocketDataProvider(&http_data);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.com", "192.168.0.1",
                                           "");
  HostResolver::RequestInfo info(HostPortPair("mail.example.com", 443));
  AddressList address;
  host_resolver_.Resolve(info, DEFAULT_PRIORITY, &address, CompletionCallback(),
                         nullptr, net_log_.bound());

  request_.url = GURL("http://mail.example.com/");
  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithConfirmationRequired) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.com", "192.168.0.1",
                                           "");
  HostResolver::RequestInfo info(HostPortPair("mail.example.com", 443));
  AddressList address;
  host_resolver_.Resolve(info, DEFAULT_PRIORITY, &address, CompletionCallback(),
                         nullptr, net_log_.bound());

  CreateSessionWithNextProtos();
  session_->quic_stream_factory()->set_require_confirmation(true);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_EQ(OK, callback.WaitForResult());

  CheckWasQuicResponse(trans);
  CheckResponseData(trans, "hello!");
}

TEST_P(QuicNetworkTransactionTest,
       LogGranularQuicErrorCodeOnQuicProtocolErrorLocal) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  // Read a close connection packet with
  // QuicErrorCode: QUIC_CRYPTO_VERSION_NOT_SUPPORTED from the peer.
  mock_quic_data.AddRead(ConstructConnectionClosePacket(1));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.com", "192.168.0.1",
                                           "");
  HostResolver::RequestInfo info(HostPortPair("mail.example.com", 443));
  AddressList address;
  host_resolver_.Resolve(info, DEFAULT_PRIORITY, &address, CompletionCallback(),
                         nullptr, net_log_.bound());

  CreateSessionWithNextProtos();
  session_->quic_stream_factory()->set_require_confirmation(true);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());

  NetErrorDetails details;
  EXPECT_EQ(QUIC_NO_ERROR, details.quic_connection_error);

  trans->PopulateNetErrorDetails(&details);
  // Verify the error code logged is what sent by the peer.
  EXPECT_EQ(QUIC_CRYPTO_VERSION_NOT_SUPPORTED, details.quic_connection_error);
}

TEST_P(QuicNetworkTransactionTest,
       LogGranularQuicErrorCodeOnQuicProtocolErrorRemote) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  // Peer sending an invalid stream frame with a invalid stream error causes
  // this end to raise error and close connection.
  mock_quic_data.AddRead(ConstructRstPacket(1, false, kClientDataStreamId1,
                                            QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Invalid rst stream error code.";
  mock_quic_data.AddWrite(ConstructAckAndConnectionClosePacket(
      2, QuicTime::Delta::Infinite(), 0, 1, QUIC_INVALID_RST_STREAM_DATA,
      quic_error_details));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.com", "192.168.0.1",
                                           "");
  HostResolver::RequestInfo info(HostPortPair("mail.example.com", 443));
  AddressList address;
  host_resolver_.Resolve(info, DEFAULT_PRIORITY, &address, CompletionCallback(),
                         nullptr, net_log_.bound());

  CreateSessionWithNextProtos();
  session_->quic_stream_factory()->set_require_confirmation(true);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());
  NetErrorDetails details;
  EXPECT_EQ(QUIC_NO_ERROR, details.quic_connection_error);

  trans->PopulateNetErrorDetails(&details);
  EXPECT_EQ(QUIC_INVALID_RST_STREAM_DATA, details.quic_connection_error);
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocol) {
  // Alternate-protocol job
  scoped_ptr<QuicEncryptedPacket> close(ConstructConnectionClosePacket(1));
  MockRead quic_reads[] = {
      MockRead(ASYNC, close->data(), close->length()),
      MockRead(ASYNC, ERR_IO_PENDING),  // No more data to read
      MockRead(ASYNC, OK),              // EOF
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolReadError) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, NoBrokenAlternateProtocolIfTcpFails) {
  // Alternate-protocol job will fail when the session attempts to read.
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job will also fail.
  MockRead http_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  http_data.set_connect_data(MockConnect(ASYNC, ERR_SOCKET_NOT_CONNECTED));
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, callback.WaitForResult());
  ExpectQuicAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, FailedZeroRttBrokenAlternateProtocol) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  AddHangingNonAlternateProtocolSocketData();

  // Second Alternate-protocol job which will race with the TCP job.
  StaticSocketDataProvider quic_data2(quic_reads, arraysize(quic_reads),
                                      nullptr, 0);
  socket_factory_.AddSocketDataProvider(&quic_data2);

  // Final job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, DISABLED_HangingZeroRttFallback) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolOnConnectFailure) {
  // Alternate-protocol job will fail before creating a QUIC session.
  StaticSocketDataProvider quic_data(nullptr, 0, nullptr, 0);
  quic_data.set_connect_data(
      MockConnect(SYNCHRONOUS, ERR_INTERNET_DISCONNECTED));
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, ConnectionCloseDuringConnect) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddSynchronousRead(ConstructConnectionClosePacket(1));
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // When the QUIC connection fails, we will try the request again over HTTP.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternateProtocolHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads), nullptr,
                                     0);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.com", "192.168.0.1",
                                           "");
  HostResolver::RequestInfo info(HostPortPair("mail.example.com", 443));
  AddressList address;
  host_resolver_.Resolve(info, DEFAULT_PRIORITY, &address, CompletionCallback(),
                         nullptr, net_log_.bound());

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, SecureResourceOverSecureQuic) {
  maker_.set_hostname("www.example.org");
  EXPECT_FALSE(rtt_observer_.rtt_notification_received());
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(ConstructResponseHeadersPacket(
      1, kClientDataStreamId1, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  request_.url = GURL("https://www.example.org:443");
  AddHangingNonAlternateProtocolSocketData();
  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectQuicResponse("hello!");
  EXPECT_TRUE(rtt_observer_.rtt_notification_received());
}

TEST_P(QuicNetworkTransactionTest, QuicUpload) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("mail.example.com:443");

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_FAILED, 1)};
  SequencedSocketData socket_data(reads, arraysize(reads), writes,
                                  arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  request_.method = "POST";
  ChunkedUploadDataStream upload_data(0);
  upload_data.AppendData("1", 1, true);

  request_.upload_data_stream = &upload_data;

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_NE(OK, callback.WaitForResult());
}

}  // namespace test
}  // namespace net
