// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_session.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_result.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/log/net_log_source.h"
#include "net/log/test_net_log.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_client_session_peer.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quic/core/crypto/aes_128_gcm_12_encrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quiche/src/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_client_promised_info_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/simple_quic_framer.h"
#include "net/third_party/quiche/src/spdy/core/spdy_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace net {
namespace test {
namespace {

const IPEndPoint kIpEndPoint = IPEndPoint(IPAddress::IPv4AllZeros(), 0);
const char kServerHostname[] = "test.example.com";
const uint16_t kServerPort = 443;
const size_t kMaxReadersPerQuicSession = 5;

struct TestParams {
  quic::ParsedQuicVersion version;
  bool client_headers_include_h2_stream_dependency;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& p) {
  return quiche::QuicheStrCat(
      ParsedQuicVersionToString(p.version), "_",
      (p.client_headers_include_h2_stream_dependency ? "" : "No"),
      "Dependency");
}

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  quic::ParsedQuicVersionVector all_supported_versions =
      quic::AllSupportedVersions();
  for (const auto& version : all_supported_versions) {
    params.push_back(TestParams{version, false});
    params.push_back(TestParams{version, true});
  }
  return params;
}

// A subclass of QuicChromiumClientSession that allows OnPathDegrading to be
// mocked.
class TestingQuicChromiumClientSession : public QuicChromiumClientSession {
 public:
  using QuicChromiumClientSession::QuicChromiumClientSession;

  MOCK_METHOD(void, OnPathDegrading, (), (override));

  void ReallyOnPathDegrading() { QuicChromiumClientSession::OnPathDegrading(); }
};

class QuicChromiumClientSessionTest
    : public ::testing::TestWithParam<TestParams>,
      public WithTaskEnvironment {
 public:
  QuicChromiumClientSessionTest()
      : version_(GetParam().version),
        client_headers_include_h2_stream_dependency_(
            GetParam().client_headers_include_h2_stream_dependency),
        crypto_config_(
            quic::test::crypto_test_utils::ProofVerifierForTesting()),
        default_read_(new MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)),
        socket_data_(
            new SequencedSocketData(base::make_span(default_read_.get(), 1),
                                    base::span<MockWrite>())),
        random_(0),
        helper_(&clock_, &random_),
        session_key_(kServerHostname,
                     kServerPort,
                     PRIVACY_MODE_DISABLED,
                     SocketTag(),
                     NetworkIsolationKey(),
                     false /* disable_secure_dns */),
        destination_(kServerHostname, kServerPort),
        client_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(&random_),
                      &clock_,
                      kServerHostname,
                      quic::Perspective::IS_CLIENT,
                      client_headers_include_h2_stream_dependency_),
        server_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(&random_),
                      &clock_,
                      kServerHostname,
                      quic::Perspective::IS_SERVER,
                      false),
        migrate_session_early_v2_(false),
        go_away_on_path_degrading_(false) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    quic::QuicEnableVersion(version_);
    // Advance the time, because timers do not like uninitialized times.
    clock_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
  }

  void ResetHandleOnError(
      std::unique_ptr<QuicChromiumClientSession::Handle>* handle,
      int net_error) {
    EXPECT_NE(OK, net_error);
    handle->reset();
  }

 protected:
  void Initialize() {
    if (socket_data_)
      socket_factory_.AddSocketDataProvider(socket_data_.get());
    std::unique_ptr<DatagramClientSocket> socket =
        socket_factory_.CreateDatagramClientSocket(DatagramSocket::DEFAULT_BIND,
                                                   &net_log_, NetLogSource());
    socket->Connect(kIpEndPoint);
    QuicChromiumPacketWriter* writer = new net::QuicChromiumPacketWriter(
        socket.get(), base::ThreadTaskRunnerHandle::Get().get());
    quic::QuicConnection* connection = new quic::QuicConnection(
        quic::QuicUtils::CreateRandomConnectionId(&random_),
        ToQuicSocketAddress(kIpEndPoint), &helper_, &alarm_factory_, writer,
        true, quic::Perspective::IS_CLIENT,
        quic::test::SupportedVersions(version_));
    session_.reset(new TestingQuicChromiumClientSession(
        connection, std::move(socket),
        /*stream_factory=*/nullptr, &crypto_client_stream_factory_, &clock_,
        &transport_security_state_, /*ssl_config_service=*/nullptr,
        base::WrapUnique(static_cast<QuicServerInfo*>(nullptr)), session_key_,
        /*require_confirmation=*/false,
        /*max_allowed_push_id=*/0, migrate_session_early_v2_,
        /*migrate_session_on_network_change_v2=*/false,
        /*defaulet_network=*/NetworkChangeNotifier::kInvalidNetworkHandle,
        quic::QuicTime::Delta::FromMilliseconds(
            kDefaultRetransmittableOnWireTimeout.InMilliseconds()),
        /*migrate_idle_session=*/false, /*allow_port_migration=*/false,
        kDefaultIdleSessionMigrationPeriod, kMaxTimeOnNonDefaultNetwork,
        kMaxMigrationsToNonDefaultNetworkOnWriteError,
        kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
        kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        go_away_on_path_degrading_,
        client_headers_include_h2_stream_dependency_,
        /*cert_verify_flags=*/0, quic::test::DefaultQuicConfig(),
        std::make_unique<TestQuicCryptoClientConfigHandle>(&crypto_config_),
        "CONNECTION_UNKNOWN", base::TimeTicks::Now(), base::TimeTicks::Now(),
        &push_promise_index_, &test_push_delegate_,
        base::DefaultTickClock::GetInstance(),
        base::ThreadTaskRunnerHandle::Get().get(),
        /*socket_performance_watcher=*/nullptr, &net_log_));

    scoped_refptr<X509Certificate> cert(
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
    verify_details_.cert_verify_result.verified_cert = cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    session_->Initialize();
    // Blackhole QPACK decoder stream instead of constructing mock writes.
    if (VersionUsesHttp3(version_.transport_version)) {
      session_->qpack_decoder()->set_qpack_stream_sender_delegate(
          &noop_qpack_stream_sender_delegate_);
    }
    session_->StartReading();
    writer->set_delegate(session_.get());
  }

  void TearDown() override {
    if (session_)
      session_->CloseSessionOnError(
          ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
          quic::ConnectionCloseBehavior::SILENT_CLOSE);
  }

  void CompleteCryptoHandshake() {
    ASSERT_THAT(session_->CryptoConnect(callback_.callback()), IsOk());
  }

  QuicChromiumPacketWriter* CreateQuicChromiumPacketWriter(
      DatagramClientSocket* socket,
      QuicChromiumClientSession* session) const {
    std::unique_ptr<QuicChromiumPacketWriter> writer(
        new QuicChromiumPacketWriter(
            socket, base::ThreadTaskRunnerHandle::Get().get()));
    writer->set_delegate(session);
    return writer.release();
  }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::QuicStreamId GetNthServerInitiatedUnidirectionalStreamId(int n) {
    return quic::test::GetNthServerInitiatedUnidirectionalStreamId(
        version_.transport_version, n);
  }

  size_t GetMaxAllowedOutgoingBidirectionalStreams() {
    quic::QuicSession* quic_session =
        dynamic_cast<quic::QuicSession*>(&*session_);
    if (!version_.HasIetfQuicFrames()) {
      return quic::test::QuicSessionPeer::GetStreamIdManager(quic_session)
          ->max_open_outgoing_streams();
    }
    // For version99, the count will include both static and dynamic streams.
    // These tests are only concerned with dynamic streams (that is, the number
    // of streams that they can create), so back out the static header stream.
    return quic::test::QuicSessionPeer::v99_streamid_manager(quic_session)
        ->max_outgoing_bidirectional_streams();
  }

  const quic::ParsedQuicVersion version_;
  const bool client_headers_include_h2_stream_dependency_;
  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  quic::QuicCryptoClientConfig crypto_config_;
  RecordingTestNetLog net_log_;
  RecordingBoundTestNetLog bound_test_net_log_;
  MockClientSocketFactory socket_factory_;
  std::unique_ptr<MockRead> default_read_;
  std::unique_ptr<SequencedSocketData> socket_data_;
  quic::MockClock clock_;
  quic::test::MockRandom random_;
  QuicChromiumConnectionHelper helper_;
  quic::test::MockAlarmFactory alarm_factory_;
  TransportSecurityState transport_security_state_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  quic::QuicClientPushPromiseIndex push_promise_index_;
  QuicSessionKey session_key_;
  HostPortPair destination_;
  std::unique_ptr<TestingQuicChromiumClientSession> session_;
  TestServerPushDelegate test_push_delegate_;
  quic::QuicConnectionVisitorInterface* visitor_;
  TestCompletionCallback callback_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  ProofVerifyDetailsChromium verify_details_;
  bool migrate_session_early_v2_;
  quic::test::NoopQpackStreamSenderDelegate noop_qpack_stream_sender_delegate_;
  bool go_away_on_path_degrading_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicChromiumClientSessionTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

// TODO(950069): Add testing for frame_origin in NetworkIsolationKey using
// kAppendInitiatingFrameOriginToNetworkIsolationKey.

TEST_P(QuicChromiumClientSessionTest, IsFatalErrorNotSetForNonFatalError) {
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  SSLInfo ssl_info;
  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.cert_status = CERT_STATUS_DATE_INVALID;
  details.is_fatal_cert_error = false;
  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_FALSE(ssl_info.is_fatal_cert_error);
}

TEST_P(QuicChromiumClientSessionTest, IsFatalErrorSetForFatalError) {
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  SSLInfo ssl_info;
  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.cert_status = CERT_STATUS_DATE_INVALID;
  details.is_fatal_cert_error = true;
  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);
  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.is_fatal_cert_error);
}

TEST_P(QuicChromiumClientSessionTest, CryptoConnect) {
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();
}

TEST_P(QuicChromiumClientSessionTest, Handle) {
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  NetLogWithSource session_net_log = session_->net_log();
  EXPECT_EQ(NetLogSourceType::QUIC_SESSION, session_net_log.source().type);
  EXPECT_EQ(&net_log_, session_net_log.net_log());

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  EXPECT_TRUE(handle->IsConnected());
  EXPECT_FALSE(handle->OneRttKeysAvailable());
  EXPECT_EQ(version_, handle->GetQuicVersion());
  EXPECT_EQ(session_key_.server_id(), handle->server_id());
  EXPECT_EQ(session_net_log.source().type, handle->net_log().source().type);
  EXPECT_EQ(session_net_log.source().id, handle->net_log().source().id);
  EXPECT_EQ(session_net_log.net_log(), handle->net_log().net_log());
  IPEndPoint address;
  EXPECT_EQ(OK, handle->GetPeerAddress(&address));
  EXPECT_EQ(kIpEndPoint, address);
  EXPECT_TRUE(handle->CreatePacketBundler().get() != nullptr);

  CompleteCryptoHandshake();

  EXPECT_TRUE(handle->OneRttKeysAvailable());

  // Request a stream and verify that a stream was created.
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/false,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());

  // Veirfy that the handle works correctly after the session is closed.
  EXPECT_FALSE(handle->IsConnected());
  EXPECT_TRUE(handle->OneRttKeysAvailable());
  EXPECT_EQ(version_, handle->GetQuicVersion());
  EXPECT_EQ(session_key_.server_id(), handle->server_id());
  EXPECT_EQ(session_net_log.source().type, handle->net_log().source().type);
  EXPECT_EQ(session_net_log.source().id, handle->net_log().source().id);
  EXPECT_EQ(session_net_log.net_log(), handle->net_log().net_log());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, handle->GetPeerAddress(&address));
  EXPECT_TRUE(handle->CreatePacketBundler().get() == nullptr);
  {
    // Verify that CreateHandle() works even after the session is closed.
    std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
        session_->CreateHandle(destination_);
    EXPECT_FALSE(handle2->IsConnected());
    EXPECT_TRUE(handle2->OneRttKeysAvailable());
    ASSERT_EQ(ERR_CONNECTION_CLOSED,
              handle2->RequestStream(/*requires_confirmation=*/false,
                                     callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  session_.reset();

  // Verify that the handle works correctly after the session is deleted.
  EXPECT_FALSE(handle->IsConnected());
  EXPECT_TRUE(handle->OneRttKeysAvailable());
  EXPECT_EQ(version_, handle->GetQuicVersion());
  EXPECT_EQ(session_key_.server_id(), handle->server_id());
  EXPECT_EQ(session_net_log.source().type, handle->net_log().source().type);
  EXPECT_EQ(session_net_log.source().id, handle->net_log().source().id);
  EXPECT_EQ(session_net_log.net_log(), handle->net_log().net_log());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, handle->GetPeerAddress(&address));
  EXPECT_TRUE(handle->CreatePacketBundler().get() == nullptr);
  ASSERT_EQ(
      ERR_CONNECTION_CLOSED,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
}

TEST_P(QuicChromiumClientSessionTest, StreamRequest) {
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Request a stream and verify that a stream was created.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/false,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConfirmationRequiredStreamRequest) {
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Request a stream and verify that a stream was created.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/true,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, StreamRequestBeforeConfirmation) {
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  // Request a stream and verify that a stream was created.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/true, callback.callback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS));

  CompleteCryptoHandshake();

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, CancelStreamRequestBeforeRelease) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, true,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Request a stream and cancel it without releasing the stream.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/false,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  handle.reset();

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, AsyncStreamRequest) {
  MockQuicData quic_data(version_);
  if (version_.HasIetfQuicFrames()) {
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
    // The open stream limit is set to 50 by
    // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
    // requested, a STREAMS_BLOCKED will be sent, indicating that it's blocked
    // at the limit of 50.
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        2, true, 50,
                                        /*unidirectional=*/false));
    quic_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(
                         3, true, GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_STREAM_CANCELLED,
                         /*include_stop_sending_if_v99=*/false));
    // After the STREAMS_BLOCKED is sent, receive a MAX_STREAMS to increase
    // the limit to 52.
    quic_data.AddRead(
        ASYNC, server_maker_.MakeMaxStreamsPacket(1, true, 52,
                                                  /*unidirectional=*/false));
  } else {
    quic_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(
                         1, true, GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_RST_ACKNOWLEDGEMENT));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close a stream and ensure the stream request completes.
  quic::QuicRstStreamFrame rst(quic::kInvalidControlFrameId,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst);
  if (version_.HasIetfQuicFrames()) {
    // For version99, to close the stream completely, we also must receive a
    // STOP_SENDING frame:
    quic::QuicStopSendingFrame stop_sending(
        quic::kInvalidControlFrameId,
        GetNthClientInitiatedBidirectionalStreamId(0),
        quic::QUIC_STREAM_CANCELLED);
    session_->OnStopSendingFrame(stop_sending);
  }
  // Pump the message loop to read the max stream id packet.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Regression test for https://crbug.com/1021938.
// When the connection is closed, there may be tasks queued in the message loop
// to read the last packet, reading that packet should not crash.
TEST_P(QuicChromiumClientSessionTest, ReadAfterConnectionClose) {
  MockQuicData quic_data(version_);
  if (version_.HasIetfQuicFrames()) {
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
    // The open stream limit is set to 50 by
    // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
    // requested, a STREAMS_BLOCKED will be sent, indicating that it's blocked
    // at the limit of 50.
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        2, true, 50,
                                        /*unidirectional=*/false));
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        3, true, 50,
                                        /*unidirectional=*/false));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  // This packet will be read after connection is closed.
  quic_data.AddRead(
      ASYNC,
      server_maker_.MakeConnectionClosePacket(
          1, false, quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!"));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request two streams which will both be pending.
  // In V99 each will generate a max stream id for each attempt.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      session_->CreateHandle(destination_);

  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(
          /*requires_confirmation=*/false,
          base::BindOnce(&QuicChromiumClientSessionTest::ResetHandleOnError,
                         base::Unretained(this), &handle2),
          TRAFFIC_ANNOTATION_FOR_TESTS));

  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            handle2->RequestStream(/*requires_confirmation=*/false,
                                   callback2.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));

  session_->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "Timed out",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // Pump the message loop to read the connection close packet.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle2.get());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ClosedWithAsyncStreamRequest) {
  MockQuicData quic_data(version_);
  if (version_.HasIetfQuicFrames()) {
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
    // The open stream limit is set to 50 by
    // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
    // requested, a STREAMS_BLOCKED will be sent, indicating that it's blocked
    // at the limit of 50.
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        2, true, 50,
                                        /*unidirectional=*/false));
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        3, true, 50,
                                        /*unidirectional=*/false));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request two streams which will both be pending.
  // In V99 each will generate a max stream id for each attempt.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      session_->CreateHandle(destination_);

  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(
          /*requires_confirmation=*/false,
          base::BindOnce(&QuicChromiumClientSessionTest::ResetHandleOnError,
                         base::Unretained(this), &handle2),
          TRAFFIC_ANNOTATION_FOR_TESTS));

  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            handle2->RequestStream(/*requires_confirmation=*/false,
                                   callback2.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));

  session_->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "Timed out",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // Pump the message loop to read the connection close packet.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle2.get());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, CancelPendingStreamRequest) {
  MockQuicData quic_data(version_);
  if (version_.HasIetfQuicFrames()) {
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
    // The open stream limit is set to 50 by
    // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
    // requested, a STREAMS_BLOCKED will be sent.
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        2, true, 50,
                                        /*unidirectional=*/false));
    // This node receives the RST_STREAM+STOP_SENDING, it responds
    // with only a RST_STREAM.
    quic_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(
                         3, true, GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_STREAM_CANCELLED,
                         /*include_stop_sending_if_v99=*/false));
  } else {
    quic_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(
                         1, true, GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_RST_ACKNOWLEDGEMENT));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Cancel the pending stream request.
  handle.reset();

  // Close a stream and ensure that no new stream is created.
  quic::QuicRstStreamFrame rst(quic::kInvalidControlFrameId,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst);
  if (version_.HasIetfQuicFrames()) {
    // For version99, we require a STOP_SENDING as well as a RESET_STREAM to
    // fully close the stream.
    quic::QuicStopSendingFrame stop_sending(
        quic::kInvalidControlFrameId,
        GetNthClientInitiatedBidirectionalStreamId(0),
        quic::QUIC_STREAM_CANCELLED);
    session_->OnStopSendingFrame(stop_sending);
  }
  EXPECT_EQ(kMaxOpenStreams - 1, session_->GetNumActiveStreams());

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConnectionCloseBeforeStreamRequest) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakePingPacket(packet_num++, true));
  quic_data.AddRead(
      ASYNC,
      server_maker_.MakeConnectionClosePacket(
          1, false, quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!"));

  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Send a ping so that client has outgoing traffic before receiving packets.
  session_->SendPing();

  // Pump the message loop to read the connection close packet.
  base::RunLoop().RunUntilIdle();

  // Request a stream and verify that it failed.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_CONNECTION_CLOSED,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConnectionCloseBeforeHandshakeConfirmed) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // TODO(nharper, b/112643533): Figure out why this test fails when TLS is
    // enabled and fix it.
    return;
  }

  // Force the connection close packet to use long headers with connection ID.
  server_maker_.SetEncryptionLevel(quic::ENCRYPTION_INITIAL);

  MockQuicData quic_data(version_);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(
      ASYNC,
      server_maker_.MakeConnectionClosePacket(
          1, false, quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!"));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/true, callback.callback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close the connection and verify that the StreamRequest completes with
  // an error.
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConnectionCloseWithPendingStreamRequest) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (version_.HasIetfQuicFrames()) {
    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakePingPacket(packet_num++, true));
  if (version_.HasIetfQuicFrames()) {
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        packet_num++, true, 50,
                                        /*unidirectional=*/false));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(
      ASYNC,
      server_maker_.MakeConnectionClosePacket(
          1, false, quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!"));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Send a ping so that client has outgoing traffic before receiving packets.
  session_->SendPing();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close the connection and verify that the StreamRequest completes with
  // an error.
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, MaxNumStreams) {
  MockQuicData quic_data(version_);
  if (version_.HasIetfQuicFrames()) {
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
    // Initial configuration is 50 dynamic streams. Taking into account
    // the static stream (headers), expect to block on when hitting the limit
    // of 50 streams
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        2, true, 50,
                                        /*unidirectional=*/false));
    quic_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(
                         3, true, GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_RST_ACKNOWLEDGEMENT));
    // For the second CreateOutgoingStream that fails because of hitting the
    // stream count limit.
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        4, true, 50,
                                        /*unidirectional=*/false));
    quic_data.AddRead(
        ASYNC, server_maker_.MakeMaxStreamsPacket(1, true, 50 + 2,
                                                  /*unidirectional=*/false));
  } else {
    quic_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(
                         1, true, GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_RST_ACKNOWLEDGEMENT));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();

  std::vector<QuicChromiumClientStream*> streams;
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientStream* stream =
        QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
    EXPECT_TRUE(stream);
    streams.push_back(stream);
  }
  // This stream, the 51st dynamic stream, can not be opened.
  EXPECT_FALSE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));

  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Close a stream and ensure I can now open a new one.
  quic::QuicStreamId stream_id = streams[0]->id();
  session_->CloseStream(stream_id);

  // Pump data, bringing in the max-stream-id
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));
  quic::QuicRstStreamFrame rst1(quic::kInvalidControlFrameId, stream_id,
                                quic::QUIC_STREAM_NO_ERROR, 0);
  session_->OnRstStream(rst1);
  EXPECT_EQ(kMaxOpenStreams - 1, session_->GetNumActiveStreams());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));
}

TEST_P(QuicChromiumClientSessionTest, PushStreamTimedOutNoResponse) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(ASYNC,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  } else if (GetParam().client_headers_include_h2_stream_dependency) {
    quic_data.AddWrite(
        ASYNC, client_maker_.MakePriorityPacket(packet_num++, true, 2, 0, 3));
  }
  quic_data.AddWrite(ASYNC, client_maker_.MakeRstPacket(
                                packet_num++, true,
                                GetNthServerInitiatedUnidirectionalStreamId(0),
                                quic::QUIC_PUSH_STREAM_TIMED_OUT));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  QuicChromiumClientStream* stream =
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  EXPECT_TRUE(stream);

  spdy::SpdyHeaderBlock promise_headers;
  promise_headers[":method"] = "GET";
  promise_headers[":authority"] = "www.example.org";
  promise_headers[":scheme"] = "https";
  promise_headers[":path"] = "/pushed.jpg";

  // Receive a PUSH PROMISE from the server.
  EXPECT_TRUE(session_->HandlePromised(
      stream->id(), GetNthServerInitiatedUnidirectionalStreamId(0),
      promise_headers));

  quic::QuicClientPromisedInfo* promised =
      session_->GetPromisedById(GetNthServerInitiatedUnidirectionalStreamId(0));
  EXPECT_TRUE(promised);
  // Fire alarm to time out the push stream.
  alarm_factory_.FireAlarm(
      quic::test::QuicClientPromisedInfoPeer::GetAlarm(promised));
  EXPECT_FALSE(
      session_->GetPromisedByUrl("https://www.example.org/pushed.jpg"));
  EXPECT_EQ(0u,
            QuicChromiumClientSessionPeer::GetPushedBytesCount(session_.get()));
  EXPECT_EQ(0u, QuicChromiumClientSessionPeer::GetPushedAndUnclaimedBytesCount(
                    session_.get()));
}

TEST_P(QuicChromiumClientSessionTest, PushStreamTimedOutWithResponse) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(ASYNC,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  } else if (GetParam().client_headers_include_h2_stream_dependency) {
    quic_data.AddWrite(
        ASYNC, client_maker_.MakePriorityPacket(packet_num++, true, 2, 0, 3));
  }
  quic_data.AddWrite(ASYNC, client_maker_.MakeRstPacket(
                                packet_num++, true,
                                GetNthServerInitiatedUnidirectionalStreamId(0),
                                quic::QUIC_PUSH_STREAM_TIMED_OUT));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  QuicChromiumClientStream* stream =
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  EXPECT_TRUE(stream);

  spdy::SpdyHeaderBlock promise_headers;
  promise_headers[":method"] = "GET";
  promise_headers[":authority"] = "www.example.org";
  promise_headers[":scheme"] = "https";
  promise_headers[":path"] = "/pushed.jpg";

  session_->GetOrCreateStream(GetNthServerInitiatedUnidirectionalStreamId(0));
  // Receive a PUSH PROMISE from the server.
  EXPECT_TRUE(session_->HandlePromised(
      stream->id(), GetNthServerInitiatedUnidirectionalStreamId(0),
      promise_headers));
  session_->OnInitialHeadersComplete(
      GetNthServerInitiatedUnidirectionalStreamId(0), spdy::SpdyHeaderBlock());
  // Read data on the pushed stream.
  quic::QuicStreamFrame data(GetNthServerInitiatedUnidirectionalStreamId(0),
                             false, 0, quiche::QuicheStringPiece("SP"));
  session_->OnStreamFrame(data);

  quic::QuicClientPromisedInfo* promised =
      session_->GetPromisedById(GetNthServerInitiatedUnidirectionalStreamId(0));
  EXPECT_TRUE(promised);
  // Fire alarm to time out the push stream.
  alarm_factory_.FireAlarm(
      quic::test::QuicClientPromisedInfoPeer::GetAlarm(promised));
  EXPECT_EQ(2u,
            QuicChromiumClientSessionPeer::GetPushedBytesCount(session_.get()));
  EXPECT_EQ(2u, QuicChromiumClientSessionPeer::GetPushedAndUnclaimedBytesCount(
                    session_.get()));
}

// Regression test for crbug.com/968621.
TEST_P(QuicChromiumClientSessionTest, PendingStreamOnRst) {
  if (!quic::VersionUsesHttp3(version_.transport_version))
    return;

  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(ASYNC,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(ASYNC, client_maker_.MakeRstPacket(
                                packet_num++, true,
                                GetNthServerInitiatedUnidirectionalStreamId(0),
                                quic::QUIC_RST_ACKNOWLEDGEMENT));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  quic::QuicStreamFrame data(GetNthServerInitiatedUnidirectionalStreamId(0),
                             false, 1, quiche::QuicheStringPiece("SP"));
  session_->OnStreamFrame(data);
  EXPECT_EQ(0u, session_->GetNumActiveStreams());
  quic::QuicRstStreamFrame rst(quic::kInvalidControlFrameId,
                               GetNthServerInitiatedUnidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst);
}

// Regression test for crbug.com/971361.
TEST_P(QuicChromiumClientSessionTest, ClosePendingStream) {
  if (!quic::VersionUsesHttp3(version_.transport_version))
    return;

  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(ASYNC,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(ASYNC, client_maker_.MakeRstPacket(
                                packet_num++, true,
                                GetNthServerInitiatedUnidirectionalStreamId(0),
                                quic::QUIC_RST_ACKNOWLEDGEMENT));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  quic::QuicStreamId id = GetNthServerInitiatedUnidirectionalStreamId(0);
  quic::QuicStreamFrame data(id, false, 1, quiche::QuicheStringPiece("SP"));
  session_->OnStreamFrame(data);
  EXPECT_EQ(0u, session_->GetNumActiveStreams());
  session_->CloseStream(id);
}

TEST_P(QuicChromiumClientSessionTest, CancelPushWhenPendingValidation) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(ASYNC,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  } else if (GetParam().client_headers_include_h2_stream_dependency) {
    quic_data.AddWrite(
        ASYNC, client_maker_.MakePriorityPacket(packet_num++, true, 2, 0, 3));
  }
  quic_data.AddWrite(ASYNC, client_maker_.MakeRstPacket(
                                packet_num++, true,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                quic::QUIC_RST_ACKNOWLEDGEMENT));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  QuicChromiumClientStream* stream =
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  EXPECT_TRUE(stream);

  spdy::SpdyHeaderBlock promise_headers;
  promise_headers[":method"] = "GET";
  promise_headers[":authority"] = "www.example.org";
  promise_headers[":scheme"] = "https";
  promise_headers[":path"] = "/pushed.jpg";

  // Receive a PUSH PROMISE from the server.
  EXPECT_TRUE(session_->HandlePromised(
      stream->id(), GetNthServerInitiatedUnidirectionalStreamId(0),
      promise_headers));

  quic::QuicClientPromisedInfo* promised =
      session_->GetPromisedById(GetNthServerInitiatedUnidirectionalStreamId(0));
  EXPECT_TRUE(promised);

  // Initiate rendezvous.
  spdy::SpdyHeaderBlock client_request = promise_headers.Clone();
  quic::test::TestPushPromiseDelegate delegate(/*match=*/true);
  promised->HandleClientRequest(client_request, &delegate);

  // Cancel the push before receiving the response to the pushed request.
  GURL pushed_url("https://www.example.org/pushed.jpg");
  test_push_delegate_.CancelPush(pushed_url);
  EXPECT_TRUE(session_->GetPromisedByUrl(pushed_url.spec()));

  // Reset the stream now before tear down.
  session_->CloseStream(GetNthClientInitiatedBidirectionalStreamId(0));
}

TEST_P(QuicChromiumClientSessionTest, CancelPushBeforeReceivingResponse) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(ASYNC,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  } else if (GetParam().client_headers_include_h2_stream_dependency) {
    quic_data.AddWrite(
        ASYNC, client_maker_.MakePriorityPacket(packet_num++, true, 2, 0, 3));
  }
  quic_data.AddWrite(ASYNC, client_maker_.MakeRstPacket(
                                packet_num++, true,
                                GetNthServerInitiatedUnidirectionalStreamId(0),
                                quic::QUIC_STREAM_CANCELLED));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  QuicChromiumClientStream* stream =
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  EXPECT_TRUE(stream);

  spdy::SpdyHeaderBlock promise_headers;
  promise_headers[":method"] = "GET";
  promise_headers[":authority"] = "www.example.org";
  promise_headers[":scheme"] = "https";
  promise_headers[":path"] = "/pushed.jpg";

  // Receive a PUSH PROMISE from the server.
  EXPECT_TRUE(session_->HandlePromised(
      stream->id(), GetNthServerInitiatedUnidirectionalStreamId(0),
      promise_headers));

  quic::QuicClientPromisedInfo* promised =
      session_->GetPromisedById(GetNthServerInitiatedUnidirectionalStreamId(0));
  EXPECT_TRUE(promised);
  // Cancel the push before receiving the response to the pushed request.
  GURL pushed_url("https://www.example.org/pushed.jpg");
  test_push_delegate_.CancelPush(pushed_url);

  EXPECT_FALSE(session_->GetPromisedByUrl(pushed_url.spec()));
  EXPECT_EQ(0u,
            QuicChromiumClientSessionPeer::GetPushedBytesCount(session_.get()));
  EXPECT_EQ(0u, QuicChromiumClientSessionPeer::GetPushedAndUnclaimedBytesCount(
                    session_.get()));
}

TEST_P(QuicChromiumClientSessionTest, CancelPushAfterReceivingResponse) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(ASYNC,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  } else if (GetParam().client_headers_include_h2_stream_dependency) {
    quic_data.AddWrite(
        ASYNC, client_maker_.MakePriorityPacket(packet_num++, true, 2, 0, 3));
  }
  quic_data.AddWrite(ASYNC, client_maker_.MakeRstPacket(
                                packet_num++, true,
                                GetNthServerInitiatedUnidirectionalStreamId(0),
                                quic::QUIC_STREAM_CANCELLED));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  QuicChromiumClientStream* stream =
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  EXPECT_TRUE(stream);

  spdy::SpdyHeaderBlock promise_headers;
  promise_headers[":method"] = "GET";
  promise_headers[":authority"] = "www.example.org";
  promise_headers[":scheme"] = "https";
  promise_headers[":path"] = "/pushed.jpg";

  session_->GetOrCreateStream(GetNthServerInitiatedUnidirectionalStreamId(0));
  // Receive a PUSH PROMISE from the server.
  EXPECT_TRUE(session_->HandlePromised(
      stream->id(), GetNthServerInitiatedUnidirectionalStreamId(0),
      promise_headers));
  session_->OnInitialHeadersComplete(
      GetNthServerInitiatedUnidirectionalStreamId(0), spdy::SpdyHeaderBlock());
  // Read data on the pushed stream.
  quic::QuicStreamFrame data(GetNthServerInitiatedUnidirectionalStreamId(0),
                             false, 0, quiche::QuicheStringPiece("SP"));
  session_->OnStreamFrame(data);

  quic::QuicClientPromisedInfo* promised =
      session_->GetPromisedById(GetNthServerInitiatedUnidirectionalStreamId(0));
  EXPECT_TRUE(promised);
  // Cancel the push after receiving data on the push stream.
  GURL pushed_url("https://www.example.org/pushed.jpg");
  test_push_delegate_.CancelPush(pushed_url);

  EXPECT_FALSE(session_->GetPromisedByUrl(pushed_url.spec()));
  EXPECT_EQ(2u,
            QuicChromiumClientSessionPeer::GetPushedBytesCount(session_.get()));
  EXPECT_EQ(2u, QuicChromiumClientSessionPeer::GetPushedAndUnclaimedBytesCount(
                    session_.get()));
}

TEST_P(QuicChromiumClientSessionTest, MaxNumStreamsViaRequest) {
  MockQuicData quic_data(version_);
  if (version_.HasIetfQuicFrames()) {
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                        2, true, 50,
                                        /*unidirectional=*/false));
    quic_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(
                         3, true, GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_RST_ACKNOWLEDGEMENT));
    quic_data.AddRead(
        ASYNC, server_maker_.MakeMaxStreamsPacket(1, true, 52,
                                                  /*unidirectional=*/false));
  } else {
    quic_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(
                         1, true, GetNthClientInitiatedBidirectionalStreamId(0),
                         quic::QUIC_RST_ACKNOWLEDGEMENT));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  std::vector<QuicChromiumClientStream*> streams;
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientStream* stream =
        QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
    EXPECT_TRUE(stream);
    streams.push_back(stream);
  }

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close a stream and ensure I can now open a new one.
  quic::QuicStreamId stream_id = streams[0]->id();
  session_->CloseStream(stream_id);
  quic::QuicRstStreamFrame rst1(quic::kInvalidControlFrameId, stream_id,
                                quic::QUIC_STREAM_NO_ERROR, 0);
  session_->OnRstStream(rst1);
  // Pump data, bringing in the max-stream-id
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);
}

TEST_P(QuicChromiumClientSessionTest, GoAwayReceived) {
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // After receiving a GoAway, I should no longer be able to create outgoing
  // streams.
  session_->connection()->OnGoAwayFrame(
      quic::QuicGoAwayFrame(quic::kInvalidControlFrameId,
                            quic::QUIC_PEER_GOING_AWAY, 1u, "Going away."));
  EXPECT_EQ(nullptr, QuicChromiumClientSessionPeer::CreateOutgoingStream(
                         session_.get()));
}

TEST_P(QuicChromiumClientSessionTest, CanPool) {
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  EXPECT_TRUE(session_->CanPool("www.example.org", PRIVACY_MODE_DISABLED,
                                SocketTag(), NetworkIsolationKey(),
                                false /* disable_secure_dns */));
  EXPECT_FALSE(session_->CanPool("www.example.org", PRIVACY_MODE_ENABLED,
                                 SocketTag(), NetworkIsolationKey(),
                                 false /* disable_secure_dns */));
  EXPECT_FALSE(session_->CanPool("www.example.org", PRIVACY_MODE_DISABLED,
                                 SocketTag(), NetworkIsolationKey(),
                                 true /* disable_secure_dns */));
#if defined(OS_ANDROID)
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
  EXPECT_FALSE(session_->CanPool("www.example.org", PRIVACY_MODE_DISABLED, tag1,
                                 NetworkIsolationKey(),
                                 false /* disable_secure_dns */));
  EXPECT_FALSE(session_->CanPool("www.example.org", PRIVACY_MODE_DISABLED, tag2,
                                 NetworkIsolationKey(),
                                 false /* disable_secure_dns */));
#endif
  EXPECT_TRUE(session_->CanPool("mail.example.org", PRIVACY_MODE_DISABLED,
                                SocketTag(), NetworkIsolationKey(),
                                false /* disable_secure_dns */));
  EXPECT_TRUE(session_->CanPool("mail.example.com", PRIVACY_MODE_DISABLED,
                                SocketTag(), NetworkIsolationKey(),
                                false /* disable_secure_dns */));
  EXPECT_FALSE(session_->CanPool("mail.google.com", PRIVACY_MODE_DISABLED,
                                 SocketTag(), NetworkIsolationKey(),
                                 false /* disable_secure_dns */));

  const auto kOriginFoo = url::Origin::Create(GURL("http://foo.test/"));

  // Check that NetworkIsolationKey is respected when feature is enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    EXPECT_TRUE(session_->CanPool("mail.example.com", PRIVACY_MODE_DISABLED,
                                  SocketTag(),
                                  NetworkIsolationKey(kOriginFoo, kOriginFoo),
                                  false /* disable_secure_dns */));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    EXPECT_FALSE(session_->CanPool("mail.example.com", PRIVACY_MODE_DISABLED,
                                   SocketTag(),
                                   NetworkIsolationKey(kOriginFoo, kOriginFoo),
                                   false /* disable_secure_dns */));
  }
}

// Much as above, but uses a non-empty NetworkIsolationKey.
TEST_P(QuicChromiumClientSessionTest, CanPoolWithNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  const auto kOriginFoo = url::Origin::Create(GURL("http://foo.test/"));
  const auto kOriginBar = url::Origin::Create(GURL("http://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kOriginFoo, kOriginFoo);
  const NetworkIsolationKey kNetworkIsolationKey2(kOriginBar, kOriginBar);

  session_key_ = QuicSessionKey(
      kServerHostname, kServerPort, PRIVACY_MODE_DISABLED, SocketTag(),
      kNetworkIsolationKey1, false /* disable_secure_dns */);

  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  EXPECT_TRUE(session_->CanPool("www.example.org", PRIVACY_MODE_DISABLED,
                                SocketTag(), kNetworkIsolationKey1,
                                false /* disable_secure_dns */));
  EXPECT_FALSE(session_->CanPool("www.example.org", PRIVACY_MODE_ENABLED,
                                 SocketTag(), kNetworkIsolationKey1,
                                 false /* disable_secure_dns */));
#if defined(OS_ANDROID)
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
  EXPECT_FALSE(session_->CanPool("www.example.org", PRIVACY_MODE_DISABLED, tag1,
                                 kNetworkIsolationKey1,
                                 false /* disable_secure_dns */));
  EXPECT_FALSE(session_->CanPool("www.example.org", PRIVACY_MODE_DISABLED, tag2,
                                 kNetworkIsolationKey1,
                                 false /* disable_secure_dns */));
#endif
  EXPECT_TRUE(session_->CanPool("mail.example.org", PRIVACY_MODE_DISABLED,
                                SocketTag(), kNetworkIsolationKey1,
                                false /* disable_secure_dns */));
  EXPECT_TRUE(session_->CanPool("mail.example.com", PRIVACY_MODE_DISABLED,
                                SocketTag(), kNetworkIsolationKey1,
                                false /* disable_secure_dns */));
  EXPECT_FALSE(session_->CanPool("mail.google.com", PRIVACY_MODE_DISABLED,
                                 SocketTag(), kNetworkIsolationKey1,
                                 false /* disable_secure_dns */));

  EXPECT_FALSE(session_->CanPool("mail.example.com", PRIVACY_MODE_DISABLED,
                                 SocketTag(), kNetworkIsolationKey2,
                                 false /* disable_secure_dns */));
  EXPECT_FALSE(session_->CanPool("mail.example.com", PRIVACY_MODE_DISABLED,
                                 SocketTag(), NetworkIsolationKey(),
                                 false /* disable_secure_dns */));
}

TEST_P(QuicChromiumClientSessionTest, ConnectionNotPooledWithDifferentPin) {
  // Configure the TransportSecurityStateSource so that kPreloadedPKPHost will
  // have static PKP pins set.
  ScopedTransportSecurityStateSource scoped_security_state_source;

  // |net::test_default::kHSTSSource| defines pins for kPreloadedPKPHost.
  // (This hostname must be in the spdy_pooling.pem SAN.)
  const char kPreloadedPKPHost[] = "www.example.org";
  // A hostname without any static state.  (This hostname isn't in
  // spdy_pooling.pem SAN, but that's okay because the
  // ProofVerifyDetailsChromium are faked.)
  const char kNoPinsHost[] = "no-pkp.example.org";

  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  transport_security_state_.EnableStaticPinsForTesting();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  uint8_t bad_pin = 3;
  details.cert_verify_result.public_key_hashes.push_back(
      GetTestHashValue(bad_pin));

  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);
  QuicChromiumClientSessionPeer::SetHostname(session_.get(), kNoPinsHost);

  EXPECT_FALSE(session_->CanPool(kPreloadedPKPHost, PRIVACY_MODE_DISABLED,
                                 SocketTag(), NetworkIsolationKey(),
                                 false /* disable_secure_dns */));
}

TEST_P(QuicChromiumClientSessionTest, ConnectionPooledWithMatchingPin) {
  ScopedTransportSecurityStateSource scoped_security_state_source;

  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  transport_security_state_.EnableStaticPinsForTesting();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  HashValue primary_pin(HASH_VALUE_SHA256);
  EXPECT_TRUE(primary_pin.FromString(
      "sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY="));
  details.cert_verify_result.public_key_hashes.push_back(primary_pin);

  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);
  QuicChromiumClientSessionPeer::SetHostname(session_.get(), "www.example.org");

  EXPECT_TRUE(session_->CanPool("mail.example.org", PRIVACY_MODE_DISABLED,
                                SocketTag(), NetworkIsolationKey(),
                                false /* disable_secure_dns */));
}

TEST_P(QuicChromiumClientSessionTest, MigrateToSocket) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  socket_data_.reset();
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  char data[] = "ABCD";
  std::unique_ptr<quic::QuicEncryptedPacket> client_ping;
  std::unique_ptr<quic::QuicEncryptedPacket> ack_and_data_out;
  if (VersionUsesHttp3(version_.transport_version)) {
    client_ping =
        client_maker_.MakeAckAndPingPacket(packet_num++, false, 1, 1, 1);
  } else {
    client_ping = client_maker_.MakePingPacket(packet_num++, true);
  }
  ack_and_data_out = client_maker_.MakeDataPacket(
      packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true, false,
      quiche::QuicheStringPiece(data));
  std::unique_ptr<quic::QuicEncryptedPacket> server_ping(
      server_maker_.MakePingPacket(1, /*include_version=*/false));
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, server_ping->data(), server_ping->length(), 0),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, client_ping->data(), client_ping->length(), 2),
      MockWrite(SYNCHRONOUS, ack_and_data_out->data(),
                ack_and_data_out->length(), 3)};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);
  // Create connected socket.
  std::unique_ptr<DatagramClientSocket> new_socket =
      socket_factory_.CreateDatagramClientSocket(DatagramSocket::DEFAULT_BIND,
                                                 &net_log_, NetLogSource());
  EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

  // Create reader and writer.
  std::unique_ptr<QuicChromiumPacketReader> new_reader(
      new QuicChromiumPacketReader(new_socket.get(), &clock_, session_.get(),
                                   kQuicYieldAfterPacketsRead,
                                   quic::QuicTime::Delta::FromMilliseconds(
                                       kQuicYieldAfterDurationMilliseconds),
                                   bound_test_net_log_.bound()));
  new_reader->StartReading();
  std::unique_ptr<QuicChromiumPacketWriter> new_writer(
      CreateQuicChromiumPacketWriter(new_socket.get(), session_.get()));

  // Migrate session.
  EXPECT_TRUE(session_->MigrateToSocket(
      std::move(new_socket), std::move(new_reader), std::move(new_writer)));
  // Spin message loop to complete migration.
  base::RunLoop().RunUntilIdle();

  // Write data to session.
  QuicChromiumClientStream* stream =
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  struct iovec iov[1];
  iov[0].iov_base = data;
  iov[0].iov_len = 4;
  quic::test::QuicStreamPeer::SendBuffer(stream).SaveStreamData(iov, 1, 0, 4);
  quic::test::QuicStreamPeer::SetStreamBytesWritten(4, stream);
  session_->WritevData(stream->id(), 4, 0, quic::NO_FIN,
                       quic::NOT_RETRANSMISSION, QuicheNullOpt);

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, MigrateToSocketMaxReaders) {
  MockQuicData quic_data(version_);
  socket_data_.reset();
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  for (size_t i = 0; i < kMaxReadersPerQuicSession; ++i) {
    MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 1)};
    std::unique_ptr<quic::QuicEncryptedPacket> ping_out(
        client_maker_.MakePingPacket(i + packet_num, /*include_version=*/true));
    MockWrite writes[] = {
        MockWrite(SYNCHRONOUS, ping_out->data(), ping_out->length(), i + 2)};
    StaticSocketDataProvider socket_data(reads, writes);
    socket_factory_.AddSocketDataProvider(&socket_data);

    // Create connected socket.
    std::unique_ptr<DatagramClientSocket> new_socket =
        socket_factory_.CreateDatagramClientSocket(DatagramSocket::DEFAULT_BIND,
                                                   &net_log_, NetLogSource());
    EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

    // Create reader and writer.
    std::unique_ptr<QuicChromiumPacketReader> new_reader(
        new QuicChromiumPacketReader(new_socket.get(), &clock_, session_.get(),
                                     kQuicYieldAfterPacketsRead,
                                     quic::QuicTime::Delta::FromMilliseconds(
                                         kQuicYieldAfterDurationMilliseconds),
                                     bound_test_net_log_.bound()));
    new_reader->StartReading();
    std::unique_ptr<QuicChromiumPacketWriter> new_writer(
        CreateQuicChromiumPacketWriter(new_socket.get(), session_.get()));

    // Migrate session.
    if (i < kMaxReadersPerQuicSession - 1) {
      EXPECT_TRUE(session_->MigrateToSocket(
          std::move(new_socket), std::move(new_reader), std::move(new_writer)));
      // Spin message loop to complete migration.
      base::RunLoop().RunUntilIdle();
      EXPECT_TRUE(socket_data.AllReadDataConsumed());
      EXPECT_TRUE(socket_data.AllWriteDataConsumed());
    } else {
      // Max readers exceeded.
      EXPECT_FALSE(session_->MigrateToSocket(
          std::move(new_socket), std::move(new_reader), std::move(new_writer)));
      EXPECT_TRUE(socket_data.AllReadDataConsumed());
      EXPECT_FALSE(socket_data.AllWriteDataConsumed());
    }
  }
}

TEST_P(QuicChromiumClientSessionTest, MigrateToSocketReadError) {
  std::unique_ptr<quic::QuicEncryptedPacket> settings_packet;
  std::unique_ptr<quic::QuicEncryptedPacket> client_ping =
      client_maker_.MakeAckAndPingPacket(2, false, 1, 1, 1);
  std::unique_ptr<quic::QuicEncryptedPacket> initial_ping;
  std::vector<MockWrite> old_writes;
  std::vector<MockRead> old_reads;
  if (VersionUsesHttp3(version_.transport_version)) {
    settings_packet = client_maker_.MakeInitialSettingsPacket(1);
    old_writes.push_back(MockWrite(ASYNC, settings_packet->data(),
                                   settings_packet->length(), 0));
  } else {
    initial_ping = client_maker_.MakePingPacket(1, true);
    old_writes.push_back(
        MockWrite(ASYNC, initial_ping->data(), initial_ping->length(), 0));
  }
  old_reads.push_back(MockRead(ASYNC, ERR_IO_PENDING, 1));
  old_reads.push_back(MockRead(ASYNC, ERR_NETWORK_CHANGED, 2));

  socket_data_.reset(new SequencedSocketData(
      base::span<const MockRead>(old_reads.data(), old_reads.size()),
      base::span<const MockWrite>(old_writes.data(), old_writes.size())));

  std::unique_ptr<quic::QuicEncryptedPacket> server_ping(
      server_maker_.MakePingPacket(1, /*include_version=*/false));
  Initialize();
  CompleteCryptoHandshake();

  if (!VersionUsesHttp3(version_.transport_version))
    session_->SendPing();
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, client_ping->data(), client_ping->length(), 1)};
  MockRead new_reads[] = {
      MockRead(SYNCHRONOUS, server_ping->data(), server_ping->length(), 0),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // pause reading.
      MockRead(ASYNC, server_ping->data(), server_ping->length(), 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // pause reading
      MockRead(ASYNC, ERR_NETWORK_CHANGED, 5)};
  SequencedSocketData new_socket_data(new_reads, writes);
  socket_factory_.AddSocketDataProvider(&new_socket_data);

  // Create connected socket.
  std::unique_ptr<DatagramClientSocket> new_socket =
      socket_factory_.CreateDatagramClientSocket(DatagramSocket::DEFAULT_BIND,
                                                 &net_log_, NetLogSource());
  EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

  // Create reader and writer.
  std::unique_ptr<QuicChromiumPacketReader> new_reader(
      new QuicChromiumPacketReader(new_socket.get(), &clock_, session_.get(),
                                   kQuicYieldAfterPacketsRead,
                                   quic::QuicTime::Delta::FromMilliseconds(
                                       kQuicYieldAfterDurationMilliseconds),
                                   bound_test_net_log_.bound()));
  new_reader->StartReading();
  std::unique_ptr<QuicChromiumPacketWriter> new_writer(
      CreateQuicChromiumPacketWriter(new_socket.get(), session_.get()));

  // Store old socket and migrate session.
  EXPECT_TRUE(session_->MigrateToSocket(
      std::move(new_socket), std::move(new_reader), std::move(new_writer)));
  // Spin message loop to complete migration.
  base::RunLoop().RunUntilIdle();

  // Read error on old socket does not impact session.
  EXPECT_TRUE(socket_data_->IsPaused());
  socket_data_->Resume();
  EXPECT_TRUE(session_->connection()->connected());
  EXPECT_TRUE(new_socket_data.IsPaused());
  new_socket_data.Resume();

  // Read error on new socket causes session close.
  EXPECT_TRUE(new_socket_data.IsPaused());
  EXPECT_TRUE(session_->connection()->connected());
  new_socket_data.Resume();
  EXPECT_FALSE(session_->connection()->connected());

  EXPECT_TRUE(socket_data_->AllReadDataConsumed());
  EXPECT_TRUE(socket_data_->AllWriteDataConsumed());
  EXPECT_TRUE(new_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(new_socket_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest,
       GoAwayOnPathDegradingOnlyWhenHandshakeConfirmed) {
  go_away_on_path_degrading_ = true;
  MockQuicData quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  session_->ReallyOnPathDegrading();
  EXPECT_FALSE(
      QuicChromiumClientSessionPeer::GetSessionGoingAway(session_.get()));
  CompleteCryptoHandshake();
  session_->ReallyOnPathDegrading();
  EXPECT_TRUE(
      QuicChromiumClientSessionPeer::GetSessionGoingAway(session_.get()));
}

TEST_P(QuicChromiumClientSessionTest, RetransmittableOnWireTimeout) {
  migrate_session_early_v2_ = true;

  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakePingPacket(packet_num++, true));

  quic_data.AddRead(ASYNC,
                    server_maker_.MakeAckPacket(1, packet_num - 1, 1, 1));

  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakePingPacket(packet_num++, false));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  EXPECT_EQ(quic::QuicTime::Delta::FromMilliseconds(200),
            session_->connection()->initial_retransmittable_on_wire_timeout());

  // Open a stream since the connection only sends PINGs to keep a
  // retransmittable packet on the wire if there's an open stream.
  EXPECT_TRUE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));

  quic::QuicAlarm* alarm =
      quic::test::QuicConnectionPeer::GetPingAlarm(session_->connection());
  EXPECT_FALSE(alarm->IsSet());

  // Send PING, which will be ACKed by the server. After the ACK, there will be
  // no retransmittable packets on the wire, so the alarm should be set.
  session_->SendPing();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(
      clock_.ApproximateNow() + quic::QuicTime::Delta::FromMilliseconds(200),
      alarm->deadline());

  // Advance clock and simulate the alarm firing. This should cause a PING to be
  // sent.
  clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(200));
  alarm_factory_.FireAlarm(alarm);
  base::RunLoop().RunUntilIdle();

  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Regression test for https://crbug.com/1043531.
TEST_P(QuicChromiumClientSessionTest, ResetOnEmptyResponseHeaders) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(ASYNC,
                       client_maker_.MakeInitialSettingsPacket(packet_num++));
  } else {
    // In case of Google QUIC, QuicSpdyStream resets the stream.
    quic_data.AddWrite(ASYNC, client_maker_.MakeRstPacket(
                                  packet_num++, true,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_HEADERS_TOO_LARGE));
  }
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);  // EOF
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  auto session_handle = session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  EXPECT_EQ(OK, session_handle->RequestStream(/*requires_confirmation=*/false,
                                              callback.callback(),
                                              TRAFFIC_ANNOTATION_FOR_TESTS));

  auto stream_handle = session_handle->ReleaseStream();
  EXPECT_TRUE(stream_handle->IsOpen());

  auto* stream = quic::test::QuicSessionPeer::GetOrCreateStream(
      session_.get(), stream_handle->id());

  const quic::QuicHeaderList empty_response_headers;
  static_cast<quic::QuicSpdyStream*>(stream)->OnStreamHeaderList(
      /* fin = */ false, /* frame_len = */ 0, empty_response_headers);

  if (VersionUsesHttp3(version_.transport_version)) {
    // In case of IETF QUIC, QuicSpdyStream::OnStreamHeaderList() calls
    // QuicChromiumClientStream::OnInitialHeadersComplete() with the empty
    // header list, and QuicChromiumClientStream signals an error.
    spdy::SpdyHeaderBlock header_block;
    int rv = stream_handle->ReadInitialHeaders(&header_block,
                                               CompletionOnceCallback());
    EXPECT_THAT(rv, IsError(ERR_INVALID_RESPONSE));
  }

  base::RunLoop().RunUntilIdle();
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

}  // namespace
}  // namespace test
}  // namespace net
