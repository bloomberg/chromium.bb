// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/properties_based_quic_server_info.h"
#include "net/quic/crypto/quic_crypto_client_config.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_packet_maker.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/test_task_runner.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_test_util.h"
#include "net/spdy/spdy_test_utils.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::ostream;
using std::string;
using std::vector;

namespace net {
namespace test {

namespace {
const char kDefaultServerHostName[] = "www.google.com";
const int kDefaultServerPort = 443;

// Run all tests with all the combinations of versions and
// enable_connection_racing.
struct TestParams {
  TestParams(const QuicVersion version, bool enable_connection_racing)
      : version(version), enable_connection_racing(enable_connection_racing) {}

  friend ostream& operator<<(ostream& os, const TestParams& p) {
    os << "{ version: " << QuicVersionToString(p.version);
    os << " enable_connection_racing: " << p.enable_connection_racing << " }";
    return os;
  }

  QuicVersion version;
  bool enable_connection_racing;
};

// Constructs various test permutations.
vector<TestParams> GetTestParams() {
  vector<TestParams> params;
  QuicVersionVector all_supported_versions = QuicSupportedVersions();
  for (const QuicVersion version : all_supported_versions) {
    params.push_back(TestParams(version, false));
    params.push_back(TestParams(version, true));
  }
  return params;
}

}  // namespace anonymous

class QuicStreamFactoryPeer {
 public:
  static QuicCryptoClientConfig* GetCryptoConfig(QuicStreamFactory* factory) {
    return &factory->crypto_config_;
  }

  static bool HasActiveSession(QuicStreamFactory* factory,
                               const HostPortPair& host_port_pair) {
    QuicServerId server_id(host_port_pair, /*is_https=*/true,
                           PRIVACY_MODE_DISABLED);
    return factory->HasActiveSession(server_id);
  }

  static QuicChromiumClientSession* GetActiveSession(
      QuicStreamFactory* factory,
      const HostPortPair& host_port_pair) {
    QuicServerId server_id(host_port_pair, /*is_https=*/true,
                           PRIVACY_MODE_DISABLED);
    DCHECK(factory->HasActiveSession(server_id));
    return factory->active_sessions_[server_id];
  }

  static scoped_ptr<QuicHttpStream> CreateFromSession(
      QuicStreamFactory* factory,
      QuicChromiumClientSession* session) {
    return factory->CreateFromSession(session);
  }

  static bool IsLiveSession(QuicStreamFactory* factory,
                            QuicChromiumClientSession* session) {
    for (QuicStreamFactory::SessionIdMap::iterator it =
             factory->all_sessions_.begin();
         it != factory->all_sessions_.end(); ++it) {
      if (it->first == session)
        return true;
    }
    return false;
  }

  static void DisableConnectionPooling(QuicStreamFactory* factory) {
    factory->disable_connection_pooling_ = true;
  }

  static void SetTaskRunner(QuicStreamFactory* factory,
                            base::TaskRunner* task_runner) {
    factory->task_runner_ = task_runner;
  }

  static void SetEnableConnectionRacing(QuicStreamFactory* factory,
                                        bool enable_connection_racing) {
    factory->enable_connection_racing_ = enable_connection_racing;
  }

  static void SetDisableDiskCache(QuicStreamFactory* factory,
                                  bool disable_disk_cache) {
    factory->disable_disk_cache_ = disable_disk_cache;
  }

  static void SetMaxNumberOfLossyConnections(
      QuicStreamFactory* factory,
      int max_number_of_lossy_connections) {
    factory->max_number_of_lossy_connections_ = max_number_of_lossy_connections;
  }

  static int GetNumberOfLossyConnections(QuicStreamFactory* factory,
                                         uint16 port) {
    return factory->number_of_lossy_connections_[port];
  }

  static bool IsQuicDisabled(QuicStreamFactory* factory, uint16 port) {
    return factory->IsQuicDisabled(port);
  }

  static bool GetDelayTcpRace(QuicStreamFactory* factory) {
    return factory->delay_tcp_race_;
  }

  static void SetDelayTcpRace(QuicStreamFactory* factory, bool delay_tcp_race) {
    factory->delay_tcp_race_ = delay_tcp_race;
  }

  static void SetYieldAfterPackets(QuicStreamFactory* factory,
                                   int yield_after_packets) {
    factory->yield_after_packets_ = yield_after_packets;
  }

  static void SetYieldAfterDuration(QuicStreamFactory* factory,
                                    QuicTime::Delta yield_after_duration) {
    factory->yield_after_duration_ = yield_after_duration;
  }

  static size_t GetNumberOfActiveJobs(QuicStreamFactory* factory,
                                      const QuicServerId& server_id) {
    return (factory->active_jobs_[server_id]).size();
  }

  static void SetThresholdTimeoutsWithOpenStreams(
      QuicStreamFactory* factory,
      int threshold_timeouts_with_open_streams) {
    factory->threshold_timeouts_with_open_streams_ =
        threshold_timeouts_with_open_streams;
  }

  static int GetNumTimeoutsWithOpenStreams(QuicStreamFactory* factory) {
    return factory->num_timeouts_with_open_streams_;
  }

  static void SetThresholdPublicResetsPostHandshake(
      QuicStreamFactory* factory,
      int threshold_public_resets_post_handshake) {
    factory->threshold_public_resets_post_handshake_ =
        threshold_public_resets_post_handshake;
  }

  static int GetNumPublicResetsPostHandshake(QuicStreamFactory* factory) {
    return factory->num_public_resets_post_handshake_;
  }

  static void MaybeInitialize(QuicStreamFactory* factory) {
    factory->MaybeInitialize();
  }

  static bool HasInitializedData(QuicStreamFactory* factory) {
    return factory->has_initialized_data_;
  }

  static bool SupportsQuicAtStartUp(QuicStreamFactory* factory,
                                    HostPortPair host_port_pair) {
    return ContainsKey(factory->quic_supported_servers_at_startup_,
                       host_port_pair);
  }

  static bool CryptoConfigCacheIsEmpty(QuicStreamFactory* factory,
                                       QuicServerId& quic_server_id) {
    return factory->CryptoConfigCacheIsEmpty(quic_server_id);
  }

  static void EnableStoreServerConfigsInProperties(QuicStreamFactory* factory) {
    factory->store_server_configs_in_properties_ = true;
  }
};

class MockQuicServerInfo : public QuicServerInfo {
 public:
  MockQuicServerInfo(const QuicServerId& server_id)
      : QuicServerInfo(server_id) {}
  ~MockQuicServerInfo() override {}

  void Start() override {}

  int WaitForDataReady(const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  void ResetWaitForDataReadyCallback() override {}

  void CancelWaitForDataReadyCallback() override {}

  bool IsDataReady() override { return false; }

  bool IsReadyToPersist() override { return false; }

  void Persist() override {}

  void OnExternalCacheHit() override {}
};

class MockQuicServerInfoFactory : public QuicServerInfoFactory {
 public:
  MockQuicServerInfoFactory() {}
  ~MockQuicServerInfoFactory() override {}

  QuicServerInfo* GetForServer(const QuicServerId& server_id) override {
    return new MockQuicServerInfo(server_id);
  }
};

class QuicStreamFactoryTest : public ::testing::TestWithParam<TestParams> {
 protected:
  QuicStreamFactoryTest()
      : random_generator_(0),
        clock_(new MockClock()),
        runner_(new TestTaskRunner(clock_)),
        maker_(GetParam().version, 0, clock_, kDefaultServerHostName),
        cert_verifier_(CertVerifier::CreateDefault()),
        channel_id_service_(
            new ChannelIDService(new DefaultChannelIDStore(nullptr),
                                 base::ThreadTaskRunnerHandle::Get())),
        factory_(&host_resolver_,
                 &socket_factory_,
                 http_server_properties_.GetWeakPtr(),
                 cert_verifier_.get(),
                 nullptr,
                 channel_id_service_.get(),
                 &transport_security_state_,
                 /*SocketPerformanceWatcherFactory*/ nullptr,
                 &crypto_client_stream_factory_,
                 &random_generator_,
                 clock_,
                 kDefaultMaxPacketSize,
                 std::string(),
                 SupportedVersions(GetParam().version),
                 /*enable_port_selection=*/true,
                 /*always_require_handshake_confirmation=*/false,
                 /*disable_connection_pooling=*/false,
                 /*load_server_info_timeout_srtt_multiplier=*/0.0f,
                 /*enable_connection_racing=*/false,
                 /*enable_non_blocking_io=*/true,
                 /*disable_disk_cache=*/false,
                 /*prefer_aes=*/false,
                 /*max_number_of_lossy_connections=*/0,
                 /*packet_loss_threshold=*/1.0f,
                 /*max_disabled_reasons=*/3,
                 /*threshold_timeouts_with_open_streams=*/2,
                 /*threshold_pulic_resets_post_handshake=*/2,
                 /*receive_buffer_size=*/0,
                 /*delay_tcp_race=*/false,
                 /*store_server_configs_in_properties=*/false,
                 QuicTagVector()),
        host_port_pair_(kDefaultServerHostName, kDefaultServerPort),
        privacy_mode_(PRIVACY_MODE_DISABLED) {
    factory_.set_require_confirmation(false);
    factory_.set_quic_server_info_factory(new MockQuicServerInfoFactory());
    clock_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    QuicStreamFactoryPeer::SetEnableConnectionRacing(
        &factory_, GetParam().enable_connection_racing);
  }

  bool HasActiveSession(const HostPortPair& host_port_pair) {
    return QuicStreamFactoryPeer::HasActiveSession(&factory_, host_port_pair);
  }

  scoped_ptr<QuicHttpStream> CreateFromSession(
      const HostPortPair& host_port_pair) {
    QuicChromiumClientSession* session =
        QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair);
    return QuicStreamFactoryPeer::CreateFromSession(&factory_, session);
  }

  int GetSourcePortForNewSession(const HostPortPair& destination) {
    return GetSourcePortForNewSessionInner(destination, false);
  }

  int GetSourcePortForNewSessionAndGoAway(
      const HostPortPair& destination) {
    return GetSourcePortForNewSessionInner(destination, true);
  }

  int GetSourcePortForNewSessionInner(const HostPortPair& destination,
                                      bool goaway_received) {
    // Should only be called if there is no active session for this destination.
    EXPECT_FALSE(HasActiveSession(destination));
    size_t socket_count = socket_factory_.udp_client_sockets().size();

    MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
    };
    DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
    socket_data.StopAfter(1);
    socket_factory_.AddSocketDataProvider(&socket_data);

    QuicStreamRequest request(&factory_);
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(destination, privacy_mode_,
                              /*cert_verify_flags=*/0, destination.host(),
                              "GET", net_log_, callback_.callback()));

    EXPECT_EQ(OK, callback_.WaitForResult());
    scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
    EXPECT_TRUE(stream.get());
    stream.reset();

    QuicChromiumClientSession* session =
        QuicStreamFactoryPeer::GetActiveSession(&factory_, destination);

    if (socket_count + 1 != socket_factory_.udp_client_sockets().size()) {
      EXPECT_TRUE(false);
      return 0;
    }

    IPEndPoint endpoint;
    socket_factory_.
        udp_client_sockets()[socket_count]->GetLocalAddress(&endpoint);
    int port = endpoint.port();
    if (goaway_received) {
      QuicGoAwayFrame goaway(QUIC_NO_ERROR, 1, "");
      session->connection()->OnGoAwayFrame(goaway);
    }

    factory_.OnSessionClosed(session);
    EXPECT_FALSE(HasActiveSession(destination));
    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());
    return port;
  }

  scoped_ptr<QuicEncryptedPacket> ConstructConnectionClosePacket(
      QuicPacketNumber num) {
    return maker_.MakeConnectionClosePacket(num);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstPacket() {
    QuicStreamId stream_id = kClientDataStreamId1;
    return maker_.MakeRstPacket(
        1, true, stream_id,
        AdjustErrorForVersion(QUIC_RST_ACKNOWLEDGEMENT, GetParam().version));
  }

  static ProofVerifyDetailsChromium DefaultProofVerifyDetails() {
    // Load a certificate that is valid for www.example.org, mail.example.org,
    // and mail.example.com.
    scoped_refptr<X509Certificate> test_cert(
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
    EXPECT_TRUE(test_cert.get());
    ProofVerifyDetailsChromium verify_details;
    verify_details.cert_verify_result.verified_cert = test_cert;
    verify_details.cert_verify_result.is_issued_by_known_root = true;
    return verify_details;
  }

  MockHostResolver host_resolver_;
  DeterministicMockClientSocketFactory socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockRandom random_generator_;
  MockClock* clock_;  // Owned by factory_.
  scoped_refptr<TestTaskRunner> runner_;
  QuicTestPacketMaker maker_;
  HttpServerPropertiesImpl http_server_properties_;
  scoped_ptr<CertVerifier> cert_verifier_;
  scoped_ptr<ChannelIDService> channel_id_service_;
  TransportSecurityState transport_security_state_;
  QuicStreamFactory factory_;
  HostPortPair host_port_pair_;
  PrivacyMode privacy_mode_;
  BoundNetLog net_log_;
  TestCompletionCallback callback_;
};

INSTANTIATE_TEST_CASE_P(Version,
                        QuicStreamFactoryTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicStreamFactoryTest, Create) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  // Will reset stream 3.
  stream = CreateFromSession(host_port_pair_);
  EXPECT_TRUE(stream.get());

  // TODO(rtenneti): We should probably have a tests that HTTP and HTTPS result
  // in streams on different sessions.
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(host_port_pair_, privacy_mode_,
                             /*cert_verify_flags=*/0, host_port_pair_.host(),
                             "GET", net_log_, callback_.callback()));
  stream = request2.ReleaseStream();  // Will reset stream 5.
  stream.reset();  // Will reset stream 7.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CreateZeroRtt) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CreateZeroRttPost) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  // Posts require handshake confirmation, so this will return asynchronously.
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "POST", net_log_, callback_.callback()));

  // Confirm the handshake and verify that the stream is created.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NoZeroRttForDifferentHost) {
  MockRead reads[] = {
      MockRead(ASYNC, OK, 0),
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  int rv = request.Request(
      host_port_pair_, privacy_mode_, /*cert_verify_flags=*/0,
      "different.host.example.com", "GET", net_log_, callback_.callback());
  // If server and origin have different hostnames, then handshake confirmation
  // should be required, so Request will return asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Confirm handshake.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_EQ(OK, callback_.WaitForResult());

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, GoAway) {
  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_);

  session->OnGoAway(QuicGoAwayFrame());

  EXPECT_FALSE(
      QuicStreamFactoryPeer::HasActiveSession(&factory_, host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, Pooling) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  HostPortPair server2("mail.google.com", kDefaultServerPort);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(
      kDefaultServerHostName, "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(
      "mail.google.com", "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_),
            QuicStreamFactoryPeer::GetActiveSession(&factory_, server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NoPoolingIfDisabled) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data1(reads, arraysize(reads), nullptr, 0);
  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data1);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data1.StopAfter(1);
  socket_data2.StopAfter(1);

  HostPortPair server2("mail.google.com", kDefaultServerPort);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(
      kDefaultServerHostName, "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(
      "mail.google.com", "192.168.0.1", "");

  // Disable connection pooling.
  QuicStreamFactoryPeer::DisableConnectionPooling(&factory_);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_),
            QuicStreamFactoryPeer::GetActiveSession(&factory_, server2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NoPoolingAfterGoAway) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data1(reads, arraysize(reads), nullptr, 0);
  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data1);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data1.StopAfter(1);
  socket_data2.StopAfter(1);

  HostPortPair server2("mail.google.com", kDefaultServerPort);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(
      kDefaultServerHostName, "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(
      "mail.google.com", "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  factory_.OnSessionGoingAway(
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::HasActiveSession(&factory_, host_port_pair_));
  EXPECT_FALSE(QuicStreamFactoryPeer::HasActiveSession(&factory_, server2));

  TestCompletionCallback callback3;
  QuicStreamRequest request3(&factory_);
  EXPECT_EQ(OK, request3.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback3.callback()));
  scoped_ptr<QuicHttpStream> stream3 = request3.ReleaseStream();
  EXPECT_TRUE(stream3.get());

  EXPECT_TRUE(QuicStreamFactoryPeer::HasActiveSession(&factory_, server2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, HttpsPooling) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  HostPortPair server1("www.example.org", 443);
  HostPortPair server2("mail.example.org", 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(server1, privacy_mode_,
                                /*cert_verify_flags=*/0, server1.host(), "GET",
                                net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(QuicStreamFactoryPeer::GetActiveSession(&factory_, server1),
            QuicStreamFactoryPeer::GetActiveSession(&factory_, server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NoHttpsPoolingIfDisabled) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data1(reads, arraysize(reads), nullptr, 0);
  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data1);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data1.StopAfter(1);
  socket_data2.StopAfter(1);

  HostPortPair server1("www.example.org", 443);
  HostPortPair server2("mail.example.org", 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  // Disable connection pooling.
  QuicStreamFactoryPeer::DisableConnectionPooling(&factory_);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(server1, privacy_mode_,
                                /*cert_verify_flags=*/0, server1.host(), "GET",
                                net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(QuicStreamFactoryPeer::GetActiveSession(&factory_, server1),
            QuicStreamFactoryPeer::GetActiveSession(&factory_, server2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

class QuicAlternativeServiceCertificateValidationPooling
    : public QuicStreamFactoryTest {
 public:
  void Run(bool valid) {
    MockRead reads[] = {
        MockRead(ASYNC, OK, 0)  // EOF
    };
    DeterministicSocketData socket_data1(reads, arraysize(reads), nullptr, 0);
    socket_factory_.AddSocketDataProvider(&socket_data1);
    socket_data1.StopAfter(1);

    HostPortPair server1("www.example.org", 443);
    HostPortPair server2("mail.example.org", 443);

    std::string origin_host(valid ? "mail.example.org" : "invalid.example.org");
    HostPortPair alternative("www.example.org", 443);

    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    bool common_name_fallback_used;
    EXPECT_EQ(valid,
              verify_details.cert_verify_result.verified_cert->VerifyNameMatch(
                  origin_host, &common_name_fallback_used));
    EXPECT_TRUE(
        verify_details.cert_verify_result.verified_cert->VerifyNameMatch(
            alternative.host(), &common_name_fallback_used));
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

    host_resolver_.set_synchronous_mode(true);
    host_resolver_.rules()->AddIPLiteralRule(alternative.host(), "192.168.0.1",
                                             "");

    // Open first stream to alternative.
    QuicStreamRequest request1(&factory_);
    EXPECT_EQ(OK, request1.Request(alternative, privacy_mode_,
                                   /*cert_verify_flags=*/0, alternative.host(),
                                   "GET", net_log_, callback_.callback()));
    scoped_ptr<QuicHttpStream> stream1 = request1.ReleaseStream();
    EXPECT_TRUE(stream1.get());

    QuicStreamRequest request2(&factory_);
    int rv = request2.Request(alternative, privacy_mode_,
                              /*cert_verify_flags=*/0, origin_host, "GET",
                              net_log_, callback_.callback());
    if (valid) {
      // Alternative service of origin to |alternative| should pool to session
      // of |stream1| even if origin is different.  Since only one
      // SocketDataProvider is set up, the second request succeeding means that
      // it pooled to the session opened by the first one.
      EXPECT_EQ(OK, rv);
      scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
      EXPECT_TRUE(stream2.get());
    } else {
      EXPECT_EQ(ERR_ALTERNATIVE_CERT_NOT_VALID_FOR_ORIGIN, rv);
    }

    EXPECT_TRUE(socket_data1.AllReadDataConsumed());
    EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  }
};

INSTANTIATE_TEST_CASE_P(Version,
                        QuicAlternativeServiceCertificateValidationPooling,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicAlternativeServiceCertificateValidationPooling, Valid) {
  Run(true);
}

TEST_P(QuicAlternativeServiceCertificateValidationPooling, Invalid) {
  Run(false);
}

TEST_P(QuicStreamFactoryTest, HttpsPoolingWithMatchingPins) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  HostPortPair server1("www.example.org", 443);
  HostPortPair server2("mail.example.org", 443);
  uint8 primary_pin = 1;
  uint8 backup_pin = 2;
  test::AddPin(&transport_security_state_, "mail.example.org", primary_pin,
               backup_pin);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  verify_details.cert_verify_result.public_key_hashes.push_back(
      test::GetTestHashValue(primary_pin));
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(server1, privacy_mode_,
                                /*cert_verify_flags=*/0, server1.host(), "GET",
                                net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(QuicStreamFactoryPeer::GetActiveSession(&factory_, server1),
            QuicStreamFactoryPeer::GetActiveSession(&factory_, server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NoHttpsPoolingWithMatchingPinsIfDisabled) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data1(reads, arraysize(reads), nullptr, 0);
  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data1);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data1.StopAfter(1);
  socket_data2.StopAfter(1);

  HostPortPair server1("www.example.org", 443);
  HostPortPair server2("mail.example.org", 443);
  uint8 primary_pin = 1;
  uint8 backup_pin = 2;
  test::AddPin(&transport_security_state_, "mail.example.org", primary_pin,
               backup_pin);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  verify_details.cert_verify_result.public_key_hashes.push_back(
      test::GetTestHashValue(primary_pin));
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  // Disable connection pooling.
  QuicStreamFactoryPeer::DisableConnectionPooling(&factory_);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(server1, privacy_mode_,
                                /*cert_verify_flags=*/0, server1.host(), "GET",
                                net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(QuicStreamFactoryPeer::GetActiveSession(&factory_, server1),
            QuicStreamFactoryPeer::GetActiveSession(&factory_, server2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NoHttpsPoolingWithDifferentPins) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data1(reads, arraysize(reads), nullptr, 0);
  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data1);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data1.StopAfter(1);
  socket_data2.StopAfter(1);

  HostPortPair server1("www.example.org", 443);
  HostPortPair server2("mail.example.org", 443);
  uint8 primary_pin = 1;
  uint8 backup_pin = 2;
  uint8 bad_pin = 3;
  test::AddPin(&transport_security_state_, "mail.example.org", primary_pin,
               backup_pin);

  ProofVerifyDetailsChromium verify_details1 = DefaultProofVerifyDetails();
  verify_details1.cert_verify_result.public_key_hashes.push_back(
      test::GetTestHashValue(bad_pin));
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  ProofVerifyDetailsChromium verify_details2 = DefaultProofVerifyDetails();
  verify_details2.cert_verify_result.public_key_hashes.push_back(
      test::GetTestHashValue(primary_pin));
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(server1, privacy_mode_,
                                /*cert_verify_flags=*/0, server1.host(), "GET",
                                net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(QuicStreamFactoryPeer::GetActiveSession(&factory_, server1),
            QuicStreamFactoryPeer::GetActiveSession(&factory_, server2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, Goaway) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_data.StopAfter(1);
  socket_factory_.AddSocketDataProvider(&socket_data);
  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_data2.StopAfter(1);
  socket_factory_.AddSocketDataProvider(&socket_data2);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  // Mark the session as going away.  Ensure that while it is still alive
  // that it is no longer active.
  QuicChromiumClientSession* session =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_);
  factory_.OnSessionGoingAway(session);
  EXPECT_EQ(true, QuicStreamFactoryPeer::IsLiveSession(&factory_, session));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::HasActiveSession(&factory_, host_port_pair_));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  // Create a new request for the same destination and verify that a
  // new session is created.
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(host_port_pair_, privacy_mode_,
                             /*cert_verify_flags=*/0, host_port_pair_.host(),
                             "GET", net_log_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(
      QuicStreamFactoryPeer::HasActiveSession(&factory_, host_port_pair_));
  EXPECT_NE(session, QuicStreamFactoryPeer::GetActiveSession(&factory_,
                                                             host_port_pair_));
  EXPECT_EQ(true, QuicStreamFactoryPeer::IsLiveSession(&factory_, session));

  stream2.reset();
  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MaxOpenStream) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  QuicStreamId stream_id = kClientDataStreamId1;
  scoped_ptr<QuicEncryptedPacket> rst(
      maker_.MakeRstPacket(1, true, stream_id, QUIC_STREAM_CANCELLED));
  MockWrite writes[] = {
    MockWrite(ASYNC, rst->data(), rst->length(), 1),
  };
  DeterministicSocketData socket_data(reads, arraysize(reads),
                                      writes, arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  HttpRequestInfo request_info;
  std::vector<QuicHttpStream*> streams;
  // The MockCryptoClientStream sets max_open_streams to be
  // kDefaultMaxStreamsPerConnection / 2.
  for (size_t i = 0; i < kDefaultMaxStreamsPerConnection / 2; i++) {
    QuicStreamRequest request(&factory_);
    int rv = request.Request(host_port_pair_, privacy_mode_,
                             /*cert_verify_flags=*/0, host_port_pair_.host(),
                             "GET", net_log_, callback_.callback());
    if (i == 0) {
      EXPECT_EQ(ERR_IO_PENDING, rv);
      EXPECT_EQ(OK, callback_.WaitForResult());
    } else {
      EXPECT_EQ(OK, rv);
    }
    scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
    EXPECT_TRUE(stream);
    EXPECT_EQ(OK, stream->InitializeStream(
        &request_info, DEFAULT_PRIORITY, net_log_, CompletionCallback()));
    streams.push_back(stream.release());
  }

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, CompletionCallback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream);
  EXPECT_EQ(ERR_IO_PENDING, stream->InitializeStream(
        &request_info, DEFAULT_PRIORITY, net_log_, callback_.callback()));

  // Close the first stream.
  streams.front()->Close(false);

  ASSERT_TRUE(callback_.have_result());

  EXPECT_EQ(OK, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  STLDeleteElements(&streams);
}

TEST_P(QuicStreamFactoryTest, ResolutionErrorInCreate) {
  DeterministicSocketData socket_data(nullptr, 0, nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);

  host_resolver_.rules()->AddSimulatedFailure(kDefaultServerHostName);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ConnectErrorInCreate) {
  MockConnect connect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  DeterministicSocketData socket_data(nullptr, 0, nullptr, 0);
  socket_data.set_connect_data(connect);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(ERR_ADDRESS_IN_USE, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CancelCreate) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  {
    QuicStreamRequest request(&factory_);
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(host_port_pair_, privacy_mode_,
                              /*cert_verify_flags=*/0, host_port_pair_.host(),
                              "GET", net_log_, callback_.callback()));
  }

  socket_data.StopAfter(1);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  scoped_ptr<QuicHttpStream> stream(CreateFromSession(host_port_pair_));
  EXPECT_TRUE(stream.get());
  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CreateConsistentEphemeralPort) {
  // Sequentially connect to the default host, then another host, and then the
  // default host.  Verify that the default host gets a consistent ephemeral
  // port, that is different from the other host's connection.

  std::string other_server_name = "other.google.com";
  EXPECT_NE(kDefaultServerHostName, other_server_name);
  HostPortPair host_port_pair2(other_server_name, kDefaultServerPort);

  int original_port = GetSourcePortForNewSession(host_port_pair_);
  EXPECT_NE(original_port, GetSourcePortForNewSession(host_port_pair2));
  EXPECT_EQ(original_port, GetSourcePortForNewSession(host_port_pair_));
}

TEST_P(QuicStreamFactoryTest, GoAwayDisablesConsistentEphemeralPort) {
  // Get a session to the host using the port suggester.
  int original_port =
      GetSourcePortForNewSessionAndGoAway(host_port_pair_);
  // Verify that the port is different after the goaway.
  EXPECT_NE(original_port, GetSourcePortForNewSession(host_port_pair_));
  // Since the previous session did not goaway we should see the original port.
  EXPECT_EQ(original_port, GetSourcePortForNewSession(host_port_pair_));
}

TEST_P(QuicStreamFactoryTest, CloseAllSessions) {
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  scoped_ptr<QuicEncryptedPacket> rst(ConstructRstPacket());
  std::vector<MockWrite> writes;
  writes.push_back(MockWrite(ASYNC, rst->data(), rst->length(), 1));
  DeterministicSocketData socket_data(reads, arraysize(reads),
                                      writes.empty() ? nullptr  : &writes[0],
                                      writes.size());
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  MockRead reads2[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data2(reads2, arraysize(reads2), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info,
                                         DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  // Close the session and verify that stream saw the error.
  factory_.CloseAllSessions(ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(ERR_INTERNET_DISCONNECTED,
            stream->ReadResponseHeaders(callback_.callback()));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(host_port_pair_, privacy_mode_,
                             /*cert_verify_flags=*/0, host_port_pair_.host(),
                             "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnIPAddressChanged) {
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  scoped_ptr<QuicEncryptedPacket> rst(ConstructRstPacket());
  std::vector<MockWrite> writes;
  writes.push_back(MockWrite(ASYNC, rst->data(), rst->length(), 1));
  DeterministicSocketData socket_data(reads, arraysize(reads),
                                      writes.empty() ? nullptr  : &writes[0],
                                      writes.size());
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  MockRead reads2[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data2(reads2, arraysize(reads2), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info,
                                         DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  // Change the IP address and verify that stream saw the error.
  factory_.OnIPAddressChanged();
  EXPECT_EQ(ERR_NETWORK_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_TRUE(factory_.require_confirmation());

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(host_port_pair_, privacy_mode_,
                             /*cert_verify_flags=*/0, host_port_pair_.host(),
                             "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnSSLConfigChanged) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, 0)  // EOF
  };
  scoped_ptr<QuicEncryptedPacket> rst(ConstructRstPacket());
  std::vector<MockWrite> writes;
  writes.push_back(MockWrite(ASYNC, rst->data(), rst->length(), 1));
  DeterministicSocketData socket_data(reads, arraysize(reads),
                                      writes.empty() ? nullptr : &writes[0],
                                      writes.size());
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  MockRead reads2[] = {
      MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data2(reads2, arraysize(reads2), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  factory_.OnSSLConfigChanged();
  EXPECT_EQ(ERR_CERT_DATABASE_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_FALSE(factory_.require_confirmation());

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(host_port_pair_, privacy_mode_,
                             /*cert_verify_flags=*/0, host_port_pair_.host(),
                             "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnCertAdded) {
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  scoped_ptr<QuicEncryptedPacket> rst(ConstructRstPacket());
  std::vector<MockWrite> writes;
  writes.push_back(MockWrite(ASYNC, rst->data(), rst->length(), 1));
  DeterministicSocketData socket_data(reads, arraysize(reads),
                                      writes.empty() ? nullptr  : &writes[0],
                                      writes.size());
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  MockRead reads2[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data2(reads2, arraysize(reads2), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info,
                                         DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  // Add a cert and verify that stream saw the event.
  factory_.OnCertAdded(nullptr);
  EXPECT_EQ(ERR_CERT_DATABASE_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_FALSE(factory_.require_confirmation());

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(host_port_pair_, privacy_mode_,
                             /*cert_verify_flags=*/0, host_port_pair_.host(),
                             "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnCACertChanged) {
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  scoped_ptr<QuicEncryptedPacket> rst(ConstructRstPacket());
  std::vector<MockWrite> writes;
  writes.push_back(MockWrite(ASYNC, rst->data(), rst->length(), 1));
  DeterministicSocketData socket_data(reads, arraysize(reads),
                                      writes.empty() ? nullptr  : &writes[0],
                                      writes.size());
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  MockRead reads2[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data2(reads2, arraysize(reads2), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info,
                                         DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  // Change the CA cert and verify that stream saw the event.
  factory_.OnCACertChanged(nullptr);
  EXPECT_EQ(ERR_CERT_DATABASE_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_FALSE(factory_.require_confirmation());

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(host_port_pair_, privacy_mode_,
                             /*cert_verify_flags=*/0, host_port_pair_.host(),
                             "GET", net_log_, callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, SharedCryptoConfig) {
  vector<string> cannoncial_suffixes;
  cannoncial_suffixes.push_back(string(".c.youtube.com"));
  cannoncial_suffixes.push_back(string(".googlevideo.com"));

  for (unsigned i = 0; i < cannoncial_suffixes.size(); ++i) {
    string r1_host_name("r1");
    string r2_host_name("r2");
    r1_host_name.append(cannoncial_suffixes[i]);
    r2_host_name.append(cannoncial_suffixes[i]);

    HostPortPair host_port_pair1(r1_host_name, 80);
    QuicCryptoClientConfig* crypto_config =
        QuicStreamFactoryPeer::GetCryptoConfig(&factory_);
    QuicServerId server_id1(host_port_pair1, /*is_https=*/true, privacy_mode_);
    QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r1_host_name);
    cached1->SetProofValid();

    HostPortPair host_port_pair2(r2_host_name, 80);
    QuicServerId server_id2(host_port_pair2, /*is_https=*/true, privacy_mode_);
    QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config->LookupOrCreate(server_id2);
    EXPECT_EQ(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->proof_valid());
  }
}

TEST_P(QuicStreamFactoryTest, CryptoConfigWhenProofIsInvalid) {
  vector<string> cannoncial_suffixes;
  cannoncial_suffixes.push_back(string(".c.youtube.com"));
  cannoncial_suffixes.push_back(string(".googlevideo.com"));

  for (unsigned i = 0; i < cannoncial_suffixes.size(); ++i) {
    string r3_host_name("r3");
    string r4_host_name("r4");
    r3_host_name.append(cannoncial_suffixes[i]);
    r4_host_name.append(cannoncial_suffixes[i]);

    HostPortPair host_port_pair1(r3_host_name, 80);
    QuicCryptoClientConfig* crypto_config =
        QuicStreamFactoryPeer::GetCryptoConfig(&factory_);
    QuicServerId server_id1(host_port_pair1, /*is_https=*/true, privacy_mode_);
    QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r3_host_name);
    cached1->SetProofInvalid();

    HostPortPair host_port_pair2(r4_host_name, 80);
    QuicServerId server_id2(host_port_pair2, /*is_https=*/true, privacy_mode_);
    QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config->LookupOrCreate(server_id2);
    EXPECT_NE(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->source_address_token().empty());
    EXPECT_FALSE(cached2->proof_valid());
  }
}

TEST_P(QuicStreamFactoryTest, RacingConnections) {
  if (!GetParam().enable_connection_racing)
    return;
  QuicStreamFactoryPeer::SetDisableDiskCache(&factory_, false);
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());

  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  MockRead reads2[] = {
      MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data2(reads2, arraysize(reads2), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  const AlternativeService alternative_service1(QUIC, host_port_pair_.host(),
                                                host_port_pair_.port());
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service1, 1.0, expiration));

  http_server_properties_.SetAlternativeServices(
      host_port_pair_, alternative_service_info_vector);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  QuicServerId server_id(host_port_pair_, /*is_https=*/true, privacy_mode_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "GET", net_log_, callback_.callback()));
  EXPECT_EQ(2u,
            QuicStreamFactoryPeer::GetNumberOfActiveJobs(&factory_, server_id));

  runner_->RunNextTask();

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_EQ(0u,
            QuicStreamFactoryPeer::GetNumberOfActiveJobs(&factory_, server_id));
}

TEST_P(QuicStreamFactoryTest, EnableNotLoadFromDiskCache) {
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  QuicStreamFactoryPeer::SetDisableDiskCache(&factory_, true);

  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  // If we are waiting for disk cache, we would have posted a task. Verify that
  // the CancelWaitForDataReady task hasn't been posted.
  ASSERT_EQ(0u, runner_->GetPostedTasks().size());

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, BadPacketLoss) {
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  QuicStreamFactoryPeer::SetDisableDiskCache(&factory_, true);
  QuicStreamFactoryPeer::SetMaxNumberOfLossyConnections(&factory_, 2);
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, host_port_pair_.port()));

  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  DeterministicSocketData socket_data2(nullptr, 0, nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  DeterministicSocketData socket_data3(nullptr, 0, nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data3);
  socket_data3.StopAfter(1);

  DeterministicSocketData socket_data4(nullptr, 0, nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data4);
  socket_data4.StopAfter(1);

  HostPortPair server2("mail.example.org", kDefaultServerPort);
  HostPortPair server3("docs.example.org", kDefaultServerPort);
  HostPortPair server4("images.example.org", kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server3.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server4.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  QuicChromiumClientSession* session =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_);

  DVLOG(1) << "Create 1st session and test packet loss";

  // Set packet_loss_rate to a lower value than packet_loss_threshold.
  EXPECT_FALSE(
      factory_.OnHandshakeConfirmed(session, /*packet_loss_rate=*/0.9f));
  EXPECT_TRUE(session->connection()->connected());
  EXPECT_TRUE(
      QuicStreamFactoryPeer::HasActiveSession(&factory_, host_port_pair_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, host_port_pair_.port()));

  // Set packet_loss_rate to a higher value than packet_loss_threshold only once
  // and that shouldn't close the session and it shouldn't disable QUIC.
  EXPECT_FALSE(
      factory_.OnHandshakeConfirmed(session, /*packet_loss_rate=*/1.0f));
  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, host_port_pair_.port()));
  EXPECT_TRUE(session->connection()->connected());
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_TRUE(
      QuicStreamFactoryPeer::HasActiveSession(&factory_, host_port_pair_));

  // Test N-in-a-row high packet loss connections.

  DVLOG(1) << "Create 2nd session and test packet loss";

  TestCompletionCallback callback2;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback2.callback()));
  QuicChromiumClientSession* session2 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server2);

  // If there is no packet loss during handshake confirmation, number of lossy
  // connections for the port should be 0.
  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, server2.port()));
  EXPECT_FALSE(
      factory_.OnHandshakeConfirmed(session2, /*packet_loss_rate=*/0.9f));
  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, server2.port()));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, server2.port()));

  // Set packet_loss_rate to a higher value than packet_loss_threshold only once
  // and that shouldn't close the session and it shouldn't disable QUIC.
  EXPECT_FALSE(
      factory_.OnHandshakeConfirmed(session2, /*packet_loss_rate=*/1.0f));
  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, server2.port()));
  EXPECT_TRUE(session2->connection()->connected());
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, server2.port()));
  EXPECT_TRUE(QuicStreamFactoryPeer::HasActiveSession(&factory_, server2));

  DVLOG(1) << "Create 3rd session which also has packet loss";

  TestCompletionCallback callback3;
  QuicStreamRequest request3(&factory_);
  EXPECT_EQ(OK, request3.Request(server3, privacy_mode_,
                                 /*cert_verify_flags=*/0, server3.host(), "GET",
                                 net_log_, callback3.callback()));
  QuicChromiumClientSession* session3 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server3);

  DVLOG(1) << "Create 4th session with packet loss and test IsQuicDisabled()";
  TestCompletionCallback callback4;
  QuicStreamRequest request4(&factory_);
  EXPECT_EQ(OK, request4.Request(server4, privacy_mode_,
                                 /*cert_verify_flags=*/0, server4.host(), "GET",
                                 net_log_, callback4.callback()));
  QuicChromiumClientSession* session4 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server4);

  // Set packet_loss_rate to higher value than packet_loss_threshold 2nd time in
  // a row and that should close the session and disable QUIC.
  EXPECT_TRUE(
      factory_.OnHandshakeConfirmed(session3, /*packet_loss_rate=*/1.0f));
  EXPECT_EQ(2, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, server3.port()));
  EXPECT_FALSE(session3->connection()->connected());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsQuicDisabled(&factory_, server3.port()));
  EXPECT_FALSE(QuicStreamFactoryPeer::HasActiveSession(&factory_, server3));
  EXPECT_FALSE(HasActiveSession(server3));

  // Set packet_loss_rate to higher value than packet_loss_threshold 3rd time in
  // a row and IsQuicDisabled() should close the session.
  EXPECT_TRUE(
      factory_.OnHandshakeConfirmed(session4, /*packet_loss_rate=*/1.0f));
  EXPECT_EQ(3, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, server4.port()));
  EXPECT_FALSE(session4->connection()->connected());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsQuicDisabled(&factory_, server4.port()));
  EXPECT_FALSE(QuicStreamFactoryPeer::HasActiveSession(&factory_, server4));
  EXPECT_FALSE(HasActiveSession(server4));

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());
  scoped_ptr<QuicHttpStream> stream3 = request3.ReleaseStream();
  EXPECT_TRUE(stream3.get());
  scoped_ptr<QuicHttpStream> stream4 = request4.ReleaseStream();
  EXPECT_TRUE(stream4.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data3.AllReadDataConsumed());
  EXPECT_TRUE(socket_data3.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data4.AllReadDataConsumed());
  EXPECT_TRUE(socket_data4.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, PublicResetPostHandshakeTwoOfTwo) {
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  QuicStreamFactoryPeer::SetDisableDiskCache(&factory_, true);
  QuicStreamFactoryPeer::SetThresholdPublicResetsPostHandshake(&factory_, 2);
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, host_port_pair_.port()));

  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  HostPortPair server2("mail.example.org", kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  QuicChromiumClientSession* session =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_);

  DVLOG(1) << "Created 1st session. Now trigger public reset post handshake";
  session->connection()->CloseConnection(QUIC_PUBLIC_RESET, true);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_EQ(1,
            QuicStreamFactoryPeer::GetNumPublicResetsPostHandshake(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  // Test two-in-a-row public reset post handshakes..
  DVLOG(1) << "Create 2nd session and trigger public reset post handshake";
  TestCompletionCallback callback2;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback2.callback()));
  QuicChromiumClientSession* session2 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server2);

  session2->connection()->CloseConnection(QUIC_PUBLIC_RESET, true);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(2,
            QuicStreamFactoryPeer::GetNumPublicResetsPostHandshake(&factory_));
  EXPECT_TRUE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(
      QuicChromiumClientSession::QUIC_DISABLED_PUBLIC_RESET_POST_HANDSHAKE,
      factory_.QuicDisabledReason(host_port_pair_.port()));

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, TimeoutsWithOpenStreamsTwoOfTwo) {
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  QuicStreamFactoryPeer::SetDisableDiskCache(&factory_, true);
  QuicStreamFactoryPeer::SetThresholdTimeoutsWithOpenStreams(&factory_, 2);
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, host_port_pair_.port()));

  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  HostPortPair server2("mail.example.org", kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  QuicChromiumClientSession* session =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_);

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  DVLOG(1)
      << "Created 1st session and initialized a stream. Now trigger timeout";
  session->connection()->CloseConnection(QUIC_CONNECTION_TIMED_OUT, false);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumTimeoutsWithOpenStreams(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  // Test two-in-a-row timeouts with open streams.
  DVLOG(1) << "Create 2nd session and timeout with open stream";
  TestCompletionCallback callback2;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback2.callback()));
  QuicChromiumClientSession* session2 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server2);

  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());
  EXPECT_EQ(OK, stream2->InitializeStream(&request_info, DEFAULT_PRIORITY,
                                          net_log_, CompletionCallback()));

  session2->connection()->CloseConnection(QUIC_CONNECTION_TIMED_OUT, false);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(2, QuicStreamFactoryPeer::GetNumTimeoutsWithOpenStreams(&factory_));
  EXPECT_TRUE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(QuicChromiumClientSession::QUIC_DISABLED_TIMEOUT_WITH_OPEN_STREAMS,
            factory_.QuicDisabledReason(host_port_pair_.port()));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, PublicResetPostHandshakeTwoOfThree) {
  QuicStreamFactoryPeer::SetDisableDiskCache(&factory_, true);
  QuicStreamFactoryPeer::SetThresholdPublicResetsPostHandshake(&factory_, 2);
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, host_port_pair_.port()));

  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  DeterministicSocketData socket_data3(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data3);
  socket_data3.StopAfter(1);

  HostPortPair server2("mail.example.org", kDefaultServerPort);
  HostPortPair server3("docs.example.org", kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server3.host(), "192.168.0.1", "");

  // Test first and third out of three public reset post handshakes.
  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  QuicChromiumClientSession* session =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_);

  DVLOG(1) << "Created 1st session. Now trigger public reset post handshake";
  session->connection()->CloseConnection(QUIC_PUBLIC_RESET, true);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_EQ(1,
            QuicStreamFactoryPeer::GetNumPublicResetsPostHandshake(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  DVLOG(1) << "Create 2nd session without disable trigger";
  TestCompletionCallback callback2;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback2.callback()));
  QuicChromiumClientSession* session2 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server2);

  session2->connection()->CloseConnection(QUIC_NO_ERROR, false);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(1,
            QuicStreamFactoryPeer::GetNumPublicResetsPostHandshake(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  DVLOG(1) << "Create 3rd session with public reset post handshake,"
           << " will disable QUIC";
  TestCompletionCallback callback3;
  QuicStreamRequest request3(&factory_);
  EXPECT_EQ(OK, request3.Request(server3, privacy_mode_,
                                 /*cert_verify_flags=*/0, server3.host(), "GET",
                                 net_log_, callback3.callback()));
  QuicChromiumClientSession* session3 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server3);

  session3->connection()->CloseConnection(QUIC_PUBLIC_RESET, true);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop3;
  run_loop3.RunUntilIdle();
  EXPECT_EQ(2,
            QuicStreamFactoryPeer::GetNumPublicResetsPostHandshake(&factory_));
  EXPECT_TRUE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(
      QuicChromiumClientSession::QUIC_DISABLED_PUBLIC_RESET_POST_HANDSHAKE,
      factory_.QuicDisabledReason(host_port_pair_.port()));

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());
  scoped_ptr<QuicHttpStream> stream3 = request3.ReleaseStream();
  EXPECT_TRUE(stream3.get());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data3.AllReadDataConsumed());
  EXPECT_TRUE(socket_data3.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, TimeoutsWithOpenStreamsTwoOfThree) {
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  QuicStreamFactoryPeer::SetDisableDiskCache(&factory_, true);
  QuicStreamFactoryPeer::SetThresholdTimeoutsWithOpenStreams(&factory_, 2);
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, host_port_pair_.port()));

  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  //  DeterministicSocketData socket_data2(nullptr, 0, nullptr, 0);
  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  DeterministicSocketData socket_data3(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data3);
  socket_data3.StopAfter(1);

  HostPortPair server2("mail.example.org", kDefaultServerPort);
  HostPortPair server3("docs.example.org", kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server3.host(), "192.168.0.1", "");

  // Test first and third out of three timeouts with open streams.
  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  QuicChromiumClientSession* session =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_);

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  DVLOG(1)
      << "Created 1st session and initialized a stream. Now trigger timeout";
  session->connection()->CloseConnection(QUIC_CONNECTION_TIMED_OUT, false);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumTimeoutsWithOpenStreams(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  // Test two-in-a-row timeouts with open streams.
  DVLOG(1) << "Create 2nd session without timeout";
  TestCompletionCallback callback2;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback2.callback()));
  QuicChromiumClientSession* session2 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server2);

  session2->connection()->CloseConnection(QUIC_NO_ERROR, true);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumTimeoutsWithOpenStreams(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  DVLOG(1) << "Create 3rd session with timeout with open streams,"
           << " will disable QUIC";

  TestCompletionCallback callback3;
  QuicStreamRequest request3(&factory_);
  EXPECT_EQ(OK, request3.Request(server3, privacy_mode_,
                                 /*cert_verify_flags=*/0, server3.host(), "GET",
                                 net_log_, callback3.callback()));
  QuicChromiumClientSession* session3 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server3);

  scoped_ptr<QuicHttpStream> stream3 = request3.ReleaseStream();
  EXPECT_TRUE(stream3.get());
  EXPECT_EQ(OK, stream3->InitializeStream(&request_info, DEFAULT_PRIORITY,
                                          net_log_, CompletionCallback()));
  session3->connection()->CloseConnection(QUIC_CONNECTION_TIMED_OUT, false);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop3;
  run_loop3.RunUntilIdle();
  EXPECT_EQ(2, QuicStreamFactoryPeer::GetNumTimeoutsWithOpenStreams(&factory_));
  EXPECT_TRUE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(QuicChromiumClientSession::QUIC_DISABLED_TIMEOUT_WITH_OPEN_STREAMS,
            factory_.QuicDisabledReason(host_port_pair_.port()));

  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data3.AllReadDataConsumed());
  EXPECT_TRUE(socket_data3.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, PublicResetPostHandshakeTwoOfFour) {
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  QuicStreamFactoryPeer::SetDisableDiskCache(&factory_, true);
  QuicStreamFactoryPeer::SetThresholdPublicResetsPostHandshake(&factory_, 2);
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, host_port_pair_.port()));

  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  DeterministicSocketData socket_data3(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data3);
  socket_data3.StopAfter(1);

  DeterministicSocketData socket_data4(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data4);
  socket_data4.StopAfter(1);

  HostPortPair server2("mail.example.org", kDefaultServerPort);
  HostPortPair server3("docs.example.org", kDefaultServerPort);
  HostPortPair server4("images.example.org", kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server3.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server4.host(), "192.168.0.1", "");

  // Test first and fourth out of four public reset post handshakes.
  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  QuicChromiumClientSession* session =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_);

  DVLOG(1) << "Created 1st session. Now trigger public reset post handshake";
  session->connection()->CloseConnection(QUIC_PUBLIC_RESET, true);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_EQ(1,
            QuicStreamFactoryPeer::GetNumPublicResetsPostHandshake(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  DVLOG(1) << "Create 2nd and 3rd sessions without disable trigger";
  TestCompletionCallback callback2;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback2.callback()));
  QuicChromiumClientSession* session2 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server2);

  session2->connection()->CloseConnection(QUIC_NO_ERROR, false);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(1,
            QuicStreamFactoryPeer::GetNumPublicResetsPostHandshake(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  TestCompletionCallback callback3;
  QuicStreamRequest request3(&factory_);
  EXPECT_EQ(OK, request3.Request(server3, privacy_mode_,
                                 /*cert_verify_flags=*/0, server3.host(), "GET",
                                 net_log_, callback3.callback()));
  QuicChromiumClientSession* session3 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server3);

  session3->connection()->CloseConnection(QUIC_NO_ERROR, false);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop3;
  run_loop3.RunUntilIdle();
  EXPECT_EQ(1,
            QuicStreamFactoryPeer::GetNumPublicResetsPostHandshake(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  DVLOG(1) << "Create 4rd session with public reset post handshake,"
           << " will not disable QUIC";
  TestCompletionCallback callback4;
  QuicStreamRequest request4(&factory_);
  EXPECT_EQ(OK, request4.Request(server4, privacy_mode_,
                                 /*cert_verify_flags=*/0, server4.host(), "GET",
                                 net_log_, callback4.callback()));
  QuicChromiumClientSession* session4 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server4);

  session4->connection()->CloseConnection(QUIC_PUBLIC_RESET, true);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop4;
  run_loop4.RunUntilIdle();
  EXPECT_EQ(1,
            QuicStreamFactoryPeer::GetNumPublicResetsPostHandshake(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());
  scoped_ptr<QuicHttpStream> stream3 = request3.ReleaseStream();
  EXPECT_TRUE(stream3.get());
  scoped_ptr<QuicHttpStream> stream4 = request4.ReleaseStream();
  EXPECT_TRUE(stream4.get());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data3.AllReadDataConsumed());
  EXPECT_TRUE(socket_data3.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data4.AllReadDataConsumed());
  EXPECT_TRUE(socket_data4.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, TimeoutsWithOpenStreamsTwoOfFour) {
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  QuicStreamFactoryPeer::SetDisableDiskCache(&factory_, true);
  QuicStreamFactoryPeer::SetThresholdTimeoutsWithOpenStreams(&factory_, 2);
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));
  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumberOfLossyConnections(
                   &factory_, host_port_pair_.port()));

  MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  //  DeterministicSocketData socket_data2(nullptr, 0, nullptr, 0);
  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  DeterministicSocketData socket_data3(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data3);
  socket_data3.StopAfter(1);

  DeterministicSocketData socket_data4(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data4);
  socket_data4.StopAfter(1);

  HostPortPair server2("mail.example.org", kDefaultServerPort);
  HostPortPair server3("docs.example.org", kDefaultServerPort);
  HostPortPair server4("images.example.org", kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server3.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server4.host(), "192.168.0.1", "");

  // Test first and fourth out of three timeouts with open streams.
  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  QuicChromiumClientSession* session =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, host_port_pair_);

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  DVLOG(1)
      << "Created 1st session and initialized a stream. Now trigger timeout";
  session->connection()->CloseConnection(QUIC_CONNECTION_TIMED_OUT, false);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumTimeoutsWithOpenStreams(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  DVLOG(1) << "Create 2nd and 3rd sessions without timeout";
  TestCompletionCallback callback2;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(server2, privacy_mode_,
                                 /*cert_verify_flags=*/0, server2.host(), "GET",
                                 net_log_, callback2.callback()));
  QuicChromiumClientSession* session2 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server2);

  session2->connection()->CloseConnection(QUIC_NO_ERROR, true);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumTimeoutsWithOpenStreams(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  TestCompletionCallback callback3;
  QuicStreamRequest request3(&factory_);
  EXPECT_EQ(OK, request3.Request(server3, privacy_mode_,
                                 /*cert_verify_flags=*/0, server3.host(), "GET",
                                 net_log_, callback3.callback()));
  QuicChromiumClientSession* session3 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server3);

  session3->connection()->CloseConnection(QUIC_NO_ERROR, true);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop3;
  run_loop3.RunUntilIdle();
  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumTimeoutsWithOpenStreams(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  DVLOG(1) << "Create 4th session with timeout with open streams,"
           << " will not disable QUIC";

  TestCompletionCallback callback4;
  QuicStreamRequest request4(&factory_);
  EXPECT_EQ(OK, request4.Request(server4, privacy_mode_,
                                 /*cert_verify_flags=*/0, server4.host(), "GET",
                                 net_log_, callback4.callback()));
  QuicChromiumClientSession* session4 =
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server4);

  scoped_ptr<QuicHttpStream> stream4 = request4.ReleaseStream();
  EXPECT_TRUE(stream4.get());
  EXPECT_EQ(OK, stream4->InitializeStream(&request_info, DEFAULT_PRIORITY,
                                          net_log_, CompletionCallback()));
  session4->connection()->CloseConnection(QUIC_CONNECTION_TIMED_OUT, false);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop4;
  run_loop4.RunUntilIdle();
  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumTimeoutsWithOpenStreams(&factory_));
  EXPECT_FALSE(
      QuicStreamFactoryPeer::IsQuicDisabled(&factory_, host_port_pair_.port()));

  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());
  scoped_ptr<QuicHttpStream> stream3 = request3.ReleaseStream();
  EXPECT_TRUE(stream3.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data3.AllReadDataConsumed());
  EXPECT_TRUE(socket_data3.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data4.AllReadDataConsumed());
  EXPECT_TRUE(socket_data4.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, EnableDelayTcpRace) {
  bool delay_tcp_race = QuicStreamFactoryPeer::GetDelayTcpRace(&factory_);
  QuicStreamFactoryPeer::SetDelayTcpRace(&factory_, false);
  MockRead reads[] = {
      MockRead(ASYNC, OK, 0),
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  http_server_properties_.SetServerNetworkStats(host_port_pair_, stats1);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, privacy_mode_,
                            /*cert_verify_flags=*/0, host_port_pair_.host(),
                            "POST", net_log_, callback_.callback()));

  // If we don't delay TCP connection, then time delay should be 0.
  EXPECT_FALSE(factory_.delay_tcp_race());
  EXPECT_EQ(base::TimeDelta(), request.GetTimeDelayForWaitingJob());

  // Enable |delay_tcp_race_| param and verify delay is one RTT and that
  // server supports QUIC.
  QuicStreamFactoryPeer::SetDelayTcpRace(&factory_, true);
  EXPECT_TRUE(factory_.delay_tcp_race());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(15),
            request.GetTimeDelayForWaitingJob());

  // Confirm the handshake and verify that the stream is created.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);

  EXPECT_EQ(OK, callback_.WaitForResult());

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  QuicStreamFactoryPeer::SetDelayTcpRace(&factory_, delay_tcp_race);
}

TEST_P(QuicStreamFactoryTest, MaybeInitialize) {
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  QuicStreamFactoryPeer::EnableStoreServerConfigsInProperties(&factory_);

  const AlternativeService alternative_service1(QUIC, host_port_pair_.host(),
                                                host_port_pair_.port());
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service1, 1.0, expiration));

  http_server_properties_.SetAlternativeServices(
      host_port_pair_, alternative_service_info_vector);

  QuicServerId quic_server_id("www.google.com", 80, false,
                              PRIVACY_MODE_DISABLED);
  QuicServerInfoFactory* quic_server_info_factory =
      new PropertiesBasedQuicServerInfoFactory(
          http_server_properties_.GetWeakPtr());
  factory_.set_quic_server_info_factory(quic_server_info_factory);

  scoped_ptr<QuicServerInfo> quic_server_info(
      quic_server_info_factory->GetForServer(quic_server_id));

  // Update quic_server_info's server_config and persist it.
  QuicServerInfo::State* state = quic_server_info->mutable_state();
  // Minimum SCFG that passes config validation checks.
  const char scfg[] = {// SCFG
                       0x53, 0x43, 0x46, 0x47,
                       // num entries
                       0x01, 0x00,
                       // padding
                       0x00, 0x00,
                       // EXPY
                       0x45, 0x58, 0x50, 0x59,
                       // EXPY end offset
                       0x08, 0x00, 0x00, 0x00,
                       // Value
                       '1', '2', '3', '4', '5', '6', '7', '8'};

  // Create temporary strings becasue Persist() clears string data in |state|.
  string server_config(reinterpret_cast<const char*>(&scfg), sizeof(scfg));
  string source_address_token("test_source_address_token");
  string signature("test_signature");
  string test_cert("test_cert");
  vector<string> certs;
  certs.push_back(test_cert);
  state->server_config = server_config;
  state->source_address_token = source_address_token;
  state->server_config_sig = signature;
  state->certs = certs;

  quic_server_info->Persist();

  QuicStreamFactoryPeer::MaybeInitialize(&factory_);
  EXPECT_TRUE(QuicStreamFactoryPeer::HasInitializedData(&factory_));
  EXPECT_TRUE(
      QuicStreamFactoryPeer::SupportsQuicAtStartUp(&factory_, host_port_pair_));
  EXPECT_FALSE(QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(&factory_,
                                                               quic_server_id));
  QuicCryptoClientConfig* crypto_config =
      QuicStreamFactoryPeer::GetCryptoConfig(&factory_);
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config->LookupOrCreate(quic_server_id);
  EXPECT_FALSE(cached->server_config().empty());
  EXPECT_TRUE(cached->GetServerConfig());
  EXPECT_EQ(server_config, cached->server_config());
  EXPECT_EQ(source_address_token, cached->source_address_token());
  EXPECT_EQ(signature, cached->signature());
  ASSERT_EQ(1U, cached->certs().size());
  EXPECT_EQ(test_cert, cached->certs()[0]);
}

TEST_P(QuicStreamFactoryTest, YieldAfterPackets) {
  QuicStreamFactoryPeer::SetYieldAfterPackets(&factory_, 0);

  scoped_ptr<QuicEncryptedPacket> close_packet(
      ConstructConnectionClosePacket(0));
  vector<MockRead> reads;
  reads.push_back(
      MockRead(SYNCHRONOUS, close_packet->data(), close_packet->length(), 0));
  reads.push_back(MockRead(ASYNC, OK, 1));
  DeterministicSocketData socket_data(&reads[0], reads.size(), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");

  // Set up the TaskObserver to verify QuicPacketReader::StartReading posts a
  // task.
  // TODO(rtenneti): Change SpdySessionTestTaskObserver to NetTestTaskObserver??
  SpdySessionTestTaskObserver observer("quic_packet_reader.cc", "StartReading");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  // Call run_loop so that QuicPacketReader::OnReadComplete() gets called.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Verify task that the observer's executed_count is 1, which indicates
  // QuicPacketReader::StartReading() has posted only one task and yielded the
  // read.
  EXPECT_EQ(1u, observer.executed_count());

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, YieldAfterDuration) {
  QuicStreamFactoryPeer::SetYieldAfterDuration(
      &factory_, QuicTime::Delta::FromMilliseconds(-1));

  scoped_ptr<QuicEncryptedPacket> close_packet(
      ConstructConnectionClosePacket(0));
  vector<MockRead> reads;
  reads.push_back(
      MockRead(SYNCHRONOUS, close_packet->data(), close_packet->length(), 0));
  reads.push_back(MockRead(ASYNC, OK, 1));
  DeterministicSocketData socket_data(&reads[0], reads.size(), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");

  // Set up the TaskObserver to verify QuicPacketReader::StartReading posts a
  // task.
  // TODO(rtenneti): Change SpdySessionTestTaskObserver to NetTestTaskObserver??
  SpdySessionTestTaskObserver observer("quic_packet_reader.cc", "StartReading");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_pair_, privacy_mode_,
                                /*cert_verify_flags=*/0, host_port_pair_.host(),
                                "GET", net_log_, callback_.callback()));

  // Call run_loop so that QuicPacketReader::OnReadComplete() gets called.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Verify task that the observer's executed_count is 1, which indicates
  // QuicPacketReader::StartReading() has posted only one task and yielded the
  // read.
  EXPECT_EQ(1u, observer.executed_count());

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

}  // namespace test
}  // namespace net
