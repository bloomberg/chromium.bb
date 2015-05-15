// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_client_stream.h"

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {
namespace {

const char kServerHostname[] = "example.com";
const uint16 kServerPort = 80;

class QuicCryptoClientStreamTest : public ::testing::Test {
 public:
  QuicCryptoClientStreamTest()
      : connection_(new PacketSavingConnection(Perspective::IS_CLIENT)),
        session_(new TestClientSession(connection_, DefaultQuicConfig())),
        server_id_(kServerHostname, kServerPort, false, PRIVACY_MODE_DISABLED),
        stream_(new QuicCryptoClientStream(server_id_,
                                           session_.get(),
                                           nullptr,
                                           &crypto_config_)) {
    session_->SetCryptoStream(stream_.get());
    // Advance the time, because timers do not like uninitialized times.
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }

  void CompleteCryptoHandshake() {
    stream_->CryptoConnect();
    CryptoTestUtils::HandshakeWithFakeServer(connection_, stream_.get());
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

  PacketSavingConnection* connection_;
  scoped_ptr<TestClientSession> session_;
  QuicServerId server_id_;
  scoped_ptr<QuicCryptoClientStream> stream_;
  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;
  QuicCryptoClientConfig crypto_config_;
};

TEST_F(QuicCryptoClientStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_->encryption_established());
  EXPECT_FALSE(stream_->handshake_confirmed());
}

TEST_F(QuicCryptoClientStreamTest, ConnectedAfterSHLO) {
  CompleteCryptoHandshake();
  EXPECT_TRUE(stream_->encryption_established());
  EXPECT_TRUE(stream_->handshake_confirmed());
}

TEST_F(QuicCryptoClientStreamTest, MessageAfterHandshake) {
  CompleteCryptoHandshake();

  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE));
  message_.set_tag(kCHLO);
  ConstructHandshakeMessage();
  stream_->ProcessRawData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoClientStreamTest, BadMessageType) {
  stream_->CryptoConnect();

  message_.set_tag(kCHLO);
  ConstructHandshakeMessage();

  EXPECT_CALL(*connection_, SendConnectionCloseWithDetails(
        QUIC_INVALID_CRYPTO_MESSAGE_TYPE, "Expected REJ"));
  stream_->ProcessRawData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoClientStreamTest, NegotiatedParameters) {
  CompleteCryptoHandshake();

  const QuicConfig* config = session_->config();
  EXPECT_EQ(kMaximumIdleTimeoutSecs,
            config->IdleConnectionStateLifetime().ToSeconds());
  EXPECT_EQ(kDefaultMaxStreamsPerConnection,
            config->MaxStreamsPerConnection());

  const QuicCryptoNegotiatedParameters& crypto_params(
      stream_->crypto_negotiated_params());
  EXPECT_EQ(crypto_config_.aead[0], crypto_params.aead);
  EXPECT_EQ(crypto_config_.kexs[0], crypto_params.key_exchange);
}

TEST_F(QuicCryptoClientStreamTest, InvalidHostname) {
  QuicServerId server_id("invalid", 80, false, PRIVACY_MODE_DISABLED);
  stream_.reset(new QuicCryptoClientStream(server_id, session_.get(), nullptr,
                                           &crypto_config_));
  session_->SetCryptoStream(stream_.get());

  CompleteCryptoHandshake();
  EXPECT_TRUE(stream_->encryption_established());
  EXPECT_TRUE(stream_->handshake_confirmed());
}

TEST_F(QuicCryptoClientStreamTest, ExpiredServerConfig) {
  // Seed the config with a cached server config.
  CompleteCryptoHandshake();

  connection_ = new PacketSavingConnection(Perspective::IS_CLIENT);
  session_.reset(new TestClientSession(connection_, DefaultQuicConfig()));
  stream_.reset(new QuicCryptoClientStream(server_id_, session_.get(), nullptr,
                                           &crypto_config_));

  session_->SetCryptoStream(stream_.get());

  // Advance time 5 years to ensure that we pass the expiry time of the cached
  // server config.
  connection_->AdvanceTime(
      QuicTime::Delta::FromSeconds(60 * 60 * 24 * 365 * 5));

  stream_->CryptoConnect();
  // Check that a client hello was sent.
  ASSERT_EQ(1u, connection_->encrypted_packets_.size());
}

TEST_F(QuicCryptoClientStreamTest, ServerConfigUpdate) {
  // Test that the crypto client stream can receive server config updates after
  // the connection has been established.
  CompleteCryptoHandshake();

  QuicCryptoClientConfig::CachedState* state =
      crypto_config_.LookupOrCreate(server_id_);

  // Ensure cached STK is different to what we send in the handshake.
  EXPECT_NE("xstk", state->source_address_token());

  // Initialize using {...} syntax to avoid trailing \0 if converting from
  // string.
  unsigned char stk[] = { 'x', 's', 't', 'k' };

  // Minimum SCFG that passes config validation checks.
  unsigned char scfg[] = {
    // SCFG
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
    '1',  '2',  '3',  '4',
    '5',  '6',  '7',  '8'
  };

  CryptoHandshakeMessage server_config_update;
  server_config_update.set_tag(kSCUP);
  server_config_update.SetValue(kSourceAddressTokenTag, stk);
  server_config_update.SetValue(kSCFG, scfg);

  scoped_ptr<QuicData> data(
      CryptoFramer::ConstructHandshakeMessage(server_config_update));
  stream_->ProcessRawData(data->data(), data->length());

  // Make sure that the STK and SCFG are cached correctly.
  EXPECT_EQ("xstk", state->source_address_token());

  string cached_scfg = state->server_config();
  test::CompareCharArraysWithHexError(
      "scfg", cached_scfg.data(), cached_scfg.length(),
      QuicUtils::AsChars(scfg), arraysize(scfg));
}

TEST_F(QuicCryptoClientStreamTest, ServerConfigUpdateBeforeHandshake) {
  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_CRYPTO_UPDATE_BEFORE_HANDSHAKE_COMPLETE));
  CryptoHandshakeMessage server_config_update;
  server_config_update.set_tag(kSCUP);
  scoped_ptr<QuicData> data(
      CryptoFramer::ConstructHandshakeMessage(server_config_update));
  stream_->ProcessRawData(data->data(), data->length());
}

class QuicCryptoClientStreamStatelessTest : public ::testing::Test {
 public:
  QuicCryptoClientStreamStatelessTest()
      : server_crypto_config_(QuicCryptoServerConfig::TESTING,
                              QuicRandom::GetInstance()),
        server_id_(kServerHostname, kServerPort, false, PRIVACY_MODE_DISABLED) {
    TestClientSession* client_session = nullptr;
    QuicCryptoClientStream* client_stream = nullptr;
    SetupCryptoClientStreamForTest(server_id_,
                                   /* supports_stateless_rejects= */ true,
                                   QuicTime::Delta::FromSeconds(100000),
                                   &client_crypto_config_, &client_connection_,
                                   &client_session, &client_stream);
    CHECK(client_session);
    CHECK(client_stream);
    client_session_.reset(client_session);
    client_stream_.reset(client_stream);
  }

  void AdvanceHandshakeWithFakeServer() {
    client_stream_->CryptoConnect();
    CryptoTestUtils::AdvanceHandshake(client_connection_, client_stream_.get(),
                                      0, server_connection_,
                                      server_stream_.get(), 0);
  }

  // Initializes the server_stream_ for stateless rejects.
  void InitializeFakeStatelessRejectServer() {
    TestServerSession* server_session = nullptr;
    QuicCryptoServerStream* server_stream = nullptr;
    SetupCryptoServerStreamForTest(server_id_,
                                   QuicTime::Delta::FromSeconds(100000),
                                   &server_crypto_config_, &server_connection_,
                                   &server_session, &server_stream);
    CHECK(server_session);
    CHECK(server_stream);
    server_session_.reset(server_session);
    server_stream_.reset(server_stream);
    CryptoTestUtils::SetupCryptoServerConfigForTest(
        server_connection_->clock(), server_connection_->random_generator(),
        server_session_->config(), &server_crypto_config_);
    server_stream_->set_use_stateless_rejects_if_peer_supported(true);
  }

  // Client crypto stream state
  PacketSavingConnection* client_connection_;
  scoped_ptr<TestClientSession> client_session_;
  scoped_ptr<QuicCryptoClientStream> client_stream_;
  QuicCryptoClientConfig client_crypto_config_;

  // Server crypto stream state
  PacketSavingConnection* server_connection_;
  scoped_ptr<TestServerSession> server_session_;
  QuicCryptoServerConfig server_crypto_config_;
  scoped_ptr<QuicCryptoServerStream> server_stream_;
  QuicServerId server_id_;
};

TEST_F(QuicCryptoClientStreamStatelessTest, StatelessReject) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stateless_reject_support,
                              true);

  QuicCryptoClientConfig::CachedState* client_state =
      client_crypto_config_.LookupOrCreate(server_id_);

  EXPECT_FALSE(client_state->has_server_designated_connection_id());
  EXPECT_CALL(*client_session_, OnProofValid(testing::_));

  InitializeFakeStatelessRejectServer();
  AdvanceHandshakeWithFakeServer();

  EXPECT_FALSE(client_stream_->encryption_established());
  EXPECT_FALSE(client_stream_->handshake_confirmed());
  // Even though the handshake was not complete, the cached client_state is
  // complete, and can be used for a subsequent successful handshake.
  EXPECT_TRUE(client_state->IsComplete(QuicWallTime::FromUNIXSeconds(0)));

  ASSERT_TRUE(client_state->has_server_designated_connection_id());
  QuicConnectionId server_designated_id =
      client_state->GetNextServerDesignatedConnectionId();
  QuicConnectionId expected_id =
      server_session_->connection()->random_generator()->RandUint64();
  EXPECT_EQ(expected_id, server_designated_id);
  EXPECT_FALSE(client_state->has_server_designated_connection_id());
}

}  // namespace
}  // namespace test
}  // namespace net
