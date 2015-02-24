// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
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
                               const HostPortPair& host_port_pair,
                               bool is_https) {
    QuicServerId server_id(host_port_pair, is_https, PRIVACY_MODE_DISABLED);
    return factory->HasActiveSession(server_id);
  }

  static QuicClientSession* GetActiveSession(
      QuicStreamFactory* factory,
      const HostPortPair& host_port_pair,
      bool is_https) {
    QuicServerId server_id(host_port_pair, is_https, PRIVACY_MODE_DISABLED);
    DCHECK(factory->HasActiveSession(server_id));
    return factory->active_sessions_[server_id];
  }

  static scoped_ptr<QuicHttpStream> CreateIfSessionExists(
      QuicStreamFactory* factory,
      const HostPortPair& host_port_pair,
      bool is_https,
      const BoundNetLog& net_log) {
    QuicServerId server_id(host_port_pair, is_https, PRIVACY_MODE_DISABLED);
    return factory->CreateIfSessionExists(server_id, net_log);
  }

  static bool IsLiveSession(QuicStreamFactory* factory,
                            QuicClientSession* session) {
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

  static void SetLoadServerInfoTimeout(QuicStreamFactory* factory,
                                       size_t load_server_info_timeout) {
    factory->load_server_info_timeout_ms_ = load_server_info_timeout;
  }

  static void SetEnableConnectionRacing(QuicStreamFactory* factory,
                                        bool enable_connection_racing) {
    factory->enable_connection_racing_ = enable_connection_racing;
  }

  static void SetDisableDiskCache(QuicStreamFactory* factory,
                                  bool disable_disk_cache) {
    factory->disable_disk_cache_ = disable_disk_cache;
  }

  static size_t GetNumberOfActiveJobs(QuicStreamFactory* factory,
                                      const QuicServerId& server_id) {
    return (factory->active_jobs_[server_id]).size();
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
        maker_(GetParam().version, 0, clock_),
        cert_verifier_(CertVerifier::CreateDefault()),
        channel_id_service_(
            new ChannelIDService(new DefaultChannelIDStore(nullptr),
                                 base::MessageLoopProxy::current())),
        factory_(&host_resolver_,
                 &socket_factory_,
                 base::WeakPtr<HttpServerProperties>(),
                 cert_verifier_.get(),
                 channel_id_service_.get(),
                 &transport_security_state_,
                 &crypto_client_stream_factory_,
                 &random_generator_,
                 clock_,
                 kDefaultMaxPacketSize,
                 std::string(),
                 SupportedVersions(GetParam().version),
                 /*enable_port_selection=*/true,
                 /*always_require_handshake_confirmation=*/false,
                 /*disable_connection_pooling=*/false,
                 /*load_server_info_timeout=*/0u,
                 /*load_server_info_timeout_srtt_multiplier=*/0.0f,
                 /*enable_truncated_connection_ids=*/true,
                 /*enable_connection_racing=*/false,
                 /*disable_disk_cache=*/false,
                 QuicTagVector()),
        host_port_pair_(kDefaultServerHostName, kDefaultServerPort),
        is_https_(false),
        privacy_mode_(PRIVACY_MODE_DISABLED) {
    factory_.set_require_confirmation(false);
    clock_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    QuicStreamFactoryPeer::SetEnableConnectionRacing(
        &factory_, GetParam().enable_connection_racing);
  }

  scoped_ptr<QuicHttpStream> CreateIfSessionExists(
      const HostPortPair& host_port_pair,
      const BoundNetLog& net_log) {
    return QuicStreamFactoryPeer::CreateIfSessionExists(
        &factory_, host_port_pair, false, net_log_);
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
    EXPECT_EQ(nullptr, CreateIfSessionExists(destination, net_log_).get());
    size_t socket_count = socket_factory_.udp_client_sockets().size();

    MockRead reads[] = {
      MockRead(ASYNC, OK, 0)  // EOF
    };
    DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
    socket_data.StopAfter(1);
    socket_factory_.AddSocketDataProvider(&socket_data);

    QuicStreamRequest request(&factory_);
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(destination,
                              is_https_,
                              privacy_mode_,
                              "GET",
                              net_log_,
                              callback_.callback()));

    EXPECT_EQ(OK, callback_.WaitForResult());
    scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
    EXPECT_TRUE(stream.get());
    stream.reset();

    QuicClientSession* session = QuicStreamFactoryPeer::GetActiveSession(
        &factory_, destination, is_https_);

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
      session->OnGoAway(goaway);
    }

    factory_.OnSessionClosed(session);
    EXPECT_EQ(nullptr, CreateIfSessionExists(destination, net_log_).get());
    EXPECT_TRUE(socket_data.at_read_eof());
    EXPECT_TRUE(socket_data.at_write_eof());
    return port;
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstPacket() {
    QuicStreamId stream_id = kClientDataStreamId1;
    return maker_.MakeRstPacket(
        1, true, stream_id,
        AdjustErrorForVersion(QUIC_RST_ACKNOWLEDGEMENT, GetParam().version));
  }

  MockQuicServerInfoFactory quic_server_info_factory_;
  MockHostResolver host_resolver_;
  DeterministicMockClientSocketFactory socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockRandom random_generator_;
  MockClock* clock_;  // Owned by factory_.
  scoped_refptr<TestTaskRunner> runner_;
  QuicTestPacketMaker maker_;
  scoped_ptr<CertVerifier> cert_verifier_;
  scoped_ptr<ChannelIDService> channel_id_service_;
  TransportSecurityState transport_security_state_;
  QuicStreamFactory factory_;
  HostPortPair host_port_pair_;
  bool is_https_;
  PrivacyMode privacy_mode_;
  BoundNetLog net_log_;
  TestCompletionCallback callback_;
};

INSTANTIATE_TEST_CASE_P(Version,
                        QuicStreamFactoryTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicStreamFactoryTest, CreateIfSessionExists) {
  EXPECT_EQ(nullptr, CreateIfSessionExists(host_port_pair_, net_log_).get());
}

TEST_P(QuicStreamFactoryTest, Create) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  // Will reset stream 3.
  stream = CreateIfSessionExists(host_port_pair_, net_log_);
  EXPECT_TRUE(stream.get());

  // TODO(rtenneti): We should probably have a tests that HTTP and HTTPS result
  // in streams on different sessions.
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(host_port_pair_,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));
  stream = request2.ReleaseStream();  // Will reset stream 5.
  stream.reset();  // Will reset stream 7.

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
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
  EXPECT_EQ(OK,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
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
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "POST",
                            net_log_,
                            callback_.callback()));

  // Confirm the handshake and verify that the stream is created.
  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

TEST_P(QuicStreamFactoryTest, CreateHttpVsHttps) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data1(reads, arraysize(reads), nullptr, 0);
  DeterministicSocketData socket_data2(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data1);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data1.StopAfter(1);
  socket_data2.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(host_port_pair_,
                             !is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  EXPECT_TRUE(stream.get());
  stream.reset();

  EXPECT_NE(QuicStreamFactoryPeer::GetActiveSession(
                &factory_, host_port_pair_, is_https_),
            QuicStreamFactoryPeer::GetActiveSession(
                &factory_, host_port_pair_, !is_https_));

  EXPECT_TRUE(socket_data1.at_read_eof());
  EXPECT_TRUE(socket_data1.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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
  EXPECT_EQ(OK,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(
      QuicStreamFactoryPeer::GetActiveSession(
          &factory_, host_port_pair_, is_https_),
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server2, is_https_));

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
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
  EXPECT_EQ(OK,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(
      QuicStreamFactoryPeer::GetActiveSession(
          &factory_, host_port_pair_, is_https_),
      QuicStreamFactoryPeer::GetActiveSession(&factory_, server2, is_https_));

  EXPECT_TRUE(socket_data1.at_read_eof());
  EXPECT_TRUE(socket_data1.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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
  EXPECT_EQ(OK,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  factory_.OnSessionGoingAway(QuicStreamFactoryPeer::GetActiveSession(
      &factory_, host_port_pair_, is_https_));
  EXPECT_FALSE(QuicStreamFactoryPeer::HasActiveSession(
      &factory_, host_port_pair_, is_https_));
  EXPECT_FALSE(QuicStreamFactoryPeer::HasActiveSession(
      &factory_, server2, is_https_));

  TestCompletionCallback callback3;
  QuicStreamRequest request3(&factory_);
  EXPECT_EQ(OK,
            request3.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback3.callback()));
  scoped_ptr<QuicHttpStream> stream3 = request3.ReleaseStream();
  EXPECT_TRUE(stream3.get());

  EXPECT_TRUE(QuicStreamFactoryPeer::HasActiveSession(
      &factory_, server2, is_https_));

  EXPECT_TRUE(socket_data1.at_read_eof());
  EXPECT_TRUE(socket_data1.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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

  // Load a cert that is valid for:
  //   www.example.org (server1)
  //   mail.example.org (server2)
  //   www.example.com
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "spdy_pooling.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = test_cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.set_proof_verify_details(&verify_details);

  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  is_https_ = true;
  EXPECT_EQ(OK,
            request.Request(server1,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server1, is_https_),
            QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server2, is_https_));

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
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

  // Load a cert that is valid for:
  //   www.example.org (server1)
  //   mail.example.org (server2)
  //   www.example.com
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "spdy_pooling.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = test_cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.set_proof_verify_details(&verify_details);

  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  // Disable connection pooling.
  QuicStreamFactoryPeer::DisableConnectionPooling(&factory_);

  QuicStreamRequest request(&factory_);
  is_https_ = true;
  EXPECT_EQ(OK,
            request.Request(server1,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server1, is_https_),
            QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server2, is_https_));

  EXPECT_TRUE(socket_data1.at_read_eof());
  EXPECT_TRUE(socket_data1.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
}

TEST_P(QuicStreamFactoryTest, NoHttpsPoolingWithCertMismatch) {
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
  HostPortPair server2("mail.google.com", 443);

  // Load a cert that is valid for:
  //   www.example.org (server1)
  //   mail.example.org
  //   www.example.com
  // But is not valid for mail.google.com (server2).
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "spdy_pooling.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = test_cert;
  crypto_client_stream_factory_.set_proof_verify_details(&verify_details);


  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  is_https_ = true;
  EXPECT_EQ(OK,
            request.Request(server1,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server1, is_https_),
            QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server2, is_https_));

  EXPECT_TRUE(socket_data1.at_read_eof());
  EXPECT_TRUE(socket_data1.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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

  // Load a cert that is valid for:
  //   www.example.org (server1)
  //   mail.example.org (server2)
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "spdy_pooling.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = test_cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  verify_details.cert_verify_result.public_key_hashes.push_back(
      test::GetTestHashValue(primary_pin));
  crypto_client_stream_factory_.set_proof_verify_details(&verify_details);


  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  is_https_ = true;
  EXPECT_EQ(OK,
            request.Request(server1,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server1, is_https_),
            QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server2, is_https_));

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
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

  // Load a cert that is valid for:
  //   www.example.org (server1)
  //   mail.example.org (server2)
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "spdy_pooling.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = test_cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  verify_details.cert_verify_result.public_key_hashes.push_back(
      test::GetTestHashValue(primary_pin));
  crypto_client_stream_factory_.set_proof_verify_details(&verify_details);


  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  // Disable connection pooling.
  QuicStreamFactoryPeer::DisableConnectionPooling(&factory_);

  QuicStreamRequest request(&factory_);
  is_https_ = true;
  EXPECT_EQ(OK,
            request.Request(server1,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server1, is_https_),
            QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server2, is_https_));

  EXPECT_TRUE(socket_data1.at_read_eof());
  EXPECT_TRUE(socket_data1.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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

  // Load a cert that is valid for:
  //   www.example.org (server1)
  //   mail.example.org (server2)
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "spdy_pooling.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = test_cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  verify_details.cert_verify_result.public_key_hashes.push_back(
      test::GetTestHashValue(bad_pin));
  crypto_client_stream_factory_.set_proof_verify_details(&verify_details);


  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_.rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  is_https_ = true;
  EXPECT_EQ(OK,
            request.Request(server1,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK,
            request2.Request(server2,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server1, is_https_),
            QuicStreamFactoryPeer::GetActiveSession(
                &factory_, server2, is_https_));

  EXPECT_TRUE(socket_data1.at_read_eof());
  EXPECT_TRUE(socket_data1.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  // Mark the session as going away.  Ensure that while it is still alive
  // that it is no longer active.
  QuicClientSession* session = QuicStreamFactoryPeer::GetActiveSession(
      &factory_, host_port_pair_, is_https_);
  factory_.OnSessionGoingAway(session);
  EXPECT_EQ(true, QuicStreamFactoryPeer::IsLiveSession(&factory_, session));
  EXPECT_FALSE(QuicStreamFactoryPeer::HasActiveSession(
      &factory_, host_port_pair_, is_https_));
  EXPECT_EQ(nullptr, CreateIfSessionExists(host_port_pair_, net_log_).get());

  // Create a new request for the same destination and verify that a
  // new session is created.
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(host_port_pair_,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(QuicStreamFactoryPeer::HasActiveSession(&factory_,
                                                      host_port_pair_,
                                                      is_https_));
  EXPECT_NE(session,
            QuicStreamFactoryPeer::GetActiveSession(
                &factory_, host_port_pair_, is_https_));
  EXPECT_EQ(true, QuicStreamFactoryPeer::IsLiveSession(&factory_, session));

  stream2.reset();
  stream.reset();

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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
    int rv = request.Request(host_port_pair_,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback());
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
  EXPECT_EQ(OK,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            CompletionCallback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream);
  EXPECT_EQ(ERR_IO_PENDING, stream->InitializeStream(
        &request_info, DEFAULT_PRIORITY, net_log_, callback_.callback()));

  // Close the first stream.
  streams.front()->Close(false);

  ASSERT_TRUE(callback_.have_result());

  EXPECT_EQ(OK, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  STLDeleteElements(&streams);
}

TEST_P(QuicStreamFactoryTest, ResolutionErrorInCreate) {
  DeterministicSocketData socket_data(nullptr, 0, nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);

  host_resolver_.rules()->AddSimulatedFailure(kDefaultServerHostName);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

TEST_P(QuicStreamFactoryTest, ConnectErrorInCreate) {
  MockConnect connect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  DeterministicSocketData socket_data(nullptr, 0, nullptr, 0);
  socket_data.set_connect_data(connect);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

  EXPECT_EQ(ERR_ADDRESS_IN_USE, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
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
              request.Request(host_port_pair_,
                              is_https_,
                              privacy_mode_,
                              "GET",
                              net_log_,
                              callback_.callback()));
  }

  socket_data.StopAfter(1);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  scoped_ptr<QuicHttpStream> stream(
      CreateIfSessionExists(host_port_pair_, net_log_));
  EXPECT_TRUE(stream.get());
  stream.reset();

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
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
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

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
            request2.Request(host_port_pair_,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

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
            request2.Request(host_port_pair_,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

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
            request2.Request(host_port_pair_,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

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
            request2.Request(host_port_pair_,
                             is_https_,
                             privacy_mode_,
                             "GET",
                             net_log_,
                             callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
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
    QuicServerId server_id1(host_port_pair1, is_https_, privacy_mode_);
    QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r1_host_name);
    cached1->SetProofValid();

    HostPortPair host_port_pair2(r2_host_name, 80);
    QuicServerId server_id2(host_port_pair2, is_https_, privacy_mode_);
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
    QuicServerId server_id1(host_port_pair1, is_https_, privacy_mode_);
    QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r3_host_name);
    cached1->SetProofInvalid();

    HostPortPair host_port_pair2(r4_host_name, 80);
    QuicServerId server_id2(host_port_pair2, is_https_, privacy_mode_);
    QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config->LookupOrCreate(server_id2);
    EXPECT_NE(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->source_address_token().empty());
    EXPECT_FALSE(cached2->proof_valid());
  }
}

TEST_P(QuicStreamFactoryTest, CancelWaitForDataReady) {
  // Don't race quic connections when testing cancel reading of server config
  // from disk cache.
  if (GetParam().enable_connection_racing)
    return;
  factory_.set_quic_server_info_factory(&quic_server_info_factory_);
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  const size_t kLoadServerInfoTimeoutMs = 50;
  QuicStreamFactoryPeer::SetLoadServerInfoTimeout(
      &factory_, kLoadServerInfoTimeoutMs);

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
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_,
                            is_https_,
                            privacy_mode_,
                            "GET",
                            net_log_,
                            callback_.callback()));

  // Verify that the CancelWaitForDataReady task has been posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(kLoadServerInfoTimeoutMs),
            runner_->GetPostedTasks()[0].delay);

  runner_->RunNextTask();
  ASSERT_EQ(0u, runner_->GetPostedTasks().size());

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

TEST_P(QuicStreamFactoryTest, RacingConnections) {
  if (!GetParam().enable_connection_racing)
    return;
  factory_.set_quic_server_info_factory(&quic_server_info_factory_);
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  const size_t kLoadServerInfoTimeoutMs = 50;
  QuicStreamFactoryPeer::SetLoadServerInfoTimeout(&factory_,
                                                  kLoadServerInfoTimeoutMs);

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

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule(host_port_pair_.host(),
                                           "192.168.0.1", "");

  QuicStreamRequest request(&factory_);
  QuicServerId server_id(host_port_pair_, is_https_, privacy_mode_);
  EXPECT_EQ(ERR_IO_PENDING,
            request.Request(host_port_pair_, is_https_, privacy_mode_, "GET",
                            net_log_, callback_.callback()));
  EXPECT_EQ(2u,
            QuicStreamFactoryPeer::GetNumberOfActiveJobs(&factory_, server_id));

  runner_->RunNextTask();

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  EXPECT_EQ(0u,
            QuicStreamFactoryPeer::GetNumberOfActiveJobs(&factory_, server_id));
}

TEST_P(QuicStreamFactoryTest, EnableNotLoadFromDiskCache) {
  factory_.set_quic_server_info_factory(&quic_server_info_factory_);
  QuicStreamFactoryPeer::SetTaskRunner(&factory_, runner_.get());
  const size_t kLoadServerInfoTimeoutMs = 50;
  QuicStreamFactoryPeer::SetLoadServerInfoTimeout(&factory_,
                                                  kLoadServerInfoTimeoutMs);
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
  EXPECT_EQ(OK, request.Request(host_port_pair_, is_https_, privacy_mode_,
                                "GET", net_log_, callback_.callback()));

  // If we are waiting for disk cache, we would have posted a task. Verify that
  // the CancelWaitForDataReady task hasn't been posted.
  ASSERT_EQ(0u, runner_->GetPostedTasks().size());

  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

}  // namespace test
}  // namespace net
