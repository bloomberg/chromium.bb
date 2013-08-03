// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_server_stream.h"

#include <map>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_server_config.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
class QuicConnection;
class ReliableQuicStream;
}  // namespace net

using testing::_;

namespace net {
namespace test {
namespace {

// TODO(agl): Use rch's utility class for parsing a message when committed.
class TestQuicVisitor : public NoOpFramerVisitor {
 public:
  TestQuicVisitor() {}

  // NoOpFramerVisitor
  virtual bool OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE {
    frame_ = frame;
    return true;
  }

  QuicStreamFrame* frame() { return &frame_; }

 private:
  QuicStreamFrame frame_;

  DISALLOW_COPY_AND_ASSIGN(TestQuicVisitor);
};

class QuicCryptoServerStreamTest : public ::testing::Test {
 public:
  QuicCryptoServerStreamTest()
      : guid_(1),
        addr_(ParseIPLiteralToNumber("192.0.2.33", &ip_) ?
              ip_ : IPAddressNumber(), 1),
        connection_(new PacketSavingConnection(guid_, addr_, true)),
        session_(connection_, QuicConfig(), true),
        crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance()),
        stream_(crypto_config_, &session_) {
    config_.SetDefaults();
    session_.config()->SetDefaults();
    session_.SetCryptoStream(&stream_);
    // We advance the clock initially because the default time is zero and the
    // strike register worries that we've just overflowed a uint32 time.
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(100000));
    // TODO(rtenneti): Enable testing of ProofSource.
    // crypto_config_.SetProofSource(CryptoTestUtils::ProofSourceForTesting());

    CryptoTestUtils::SetupCryptoServerConfigForTest(
        connection_->clock(), connection_->random_generator(),
        session_.config(), &crypto_config_);
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

  int CompleteCryptoHandshake() {
    return CryptoTestUtils::HandshakeWithFakeClient(connection_, &stream_,
                                                    client_options_);
  }

 protected:
  IPAddressNumber ip_;
  QuicGuid guid_;
  IPEndPoint addr_;
  PacketSavingConnection* connection_;
  TestSession session_;
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  QuicCryptoServerStream stream_;
  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;
  CryptoTestUtils::FakeClientOptions client_options_;
};

TEST_F(QuicCryptoServerStreamTest, NotInitiallyConected) {
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  EXPECT_FALSE(stream_.encryption_established());
  EXPECT_FALSE(stream_.handshake_confirmed());
}

TEST_F(QuicCryptoServerStreamTest, ConnectedAfterCHLO) {
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  // CompleteCryptoHandshake returns the number of client hellos sent. This
  // test should send:
  //   * One to get a source-address token and certificates.
  //   * One to complete the handshake.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(stream_.encryption_established());
  EXPECT_TRUE(stream_.handshake_confirmed());
}

TEST_F(QuicCryptoServerStreamTest, ZeroRTT) {
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  QuicGuid guid(1);
  IPAddressNumber ip;
  ParseIPLiteralToNumber("127.0.0.1", &ip);
  IPEndPoint addr(ip, 0);
  PacketSavingConnection* client_conn =
      new PacketSavingConnection(guid, addr, false);
  PacketSavingConnection* server_conn =
      new PacketSavingConnection(guid, addr, false);
  client_conn->AdvanceTime(QuicTime::Delta::FromSeconds(100000));
  server_conn->AdvanceTime(QuicTime::Delta::FromSeconds(100000));

  QuicConfig client_config;
  client_config.SetDefaults();
  scoped_ptr<TestSession> client_session(
      new TestSession(client_conn, client_config, false));
  QuicCryptoClientConfig client_crypto_config;
  client_crypto_config.SetDefaults();

  scoped_ptr<QuicCryptoClientStream> client(new QuicCryptoClientStream(
        "test.example.com", client_session.get(), &client_crypto_config));
  client_session->SetCryptoStream(client.get());

  // Do a first handshake in order to prime the client config with the server's
  // information.
  CHECK(client->CryptoConnect());
  CHECK_EQ(1u, client_conn->packets_.size());

  scoped_ptr<TestSession> server_session(
      new TestSession(server_conn, config_, true));
  scoped_ptr<QuicCryptoServerStream> server(
      new QuicCryptoServerStream(crypto_config_, server_session.get()));
  server_session->SetCryptoStream(server.get());

  CryptoTestUtils::CommunicateHandshakeMessages(
      client_conn, client.get(), server_conn, server.get());
  EXPECT_EQ(2, client->num_sent_client_hellos());

  // Now do another handshake, hopefully in 0-RTT.
  LOG(INFO) << "Resetting for 0-RTT handshake attempt";

  client_conn = new PacketSavingConnection(guid, addr, false);
  server_conn = new PacketSavingConnection(guid, addr, false);
  // We need to advance time past the strike-server window so that it's
  // authoritative in this time span.
  client_conn->AdvanceTime(QuicTime::Delta::FromSeconds(102000));
  server_conn->AdvanceTime(QuicTime::Delta::FromSeconds(102000));

  // This causes the client's nonce to be different and thus stops the
  // strike-register from rejecting the repeated nonce.
  reinterpret_cast<MockRandom*>(client_conn->random_generator())->ChangeValue();
  client_session.reset(new TestSession(client_conn, client_config, false));
  server_session.reset(new TestSession(server_conn, config_, true));
  client.reset(new QuicCryptoClientStream(
        "test.example.com", client_session.get(), &client_crypto_config));
  client_session->SetCryptoStream(client.get());

  server.reset(new QuicCryptoServerStream(crypto_config_,
                                          server_session.get()));
  server_session->SetCryptoStream(server.get());

  CHECK(client->CryptoConnect());

  CryptoTestUtils::CommunicateHandshakeMessages(
      client_conn, client.get(), server_conn, server.get());
  EXPECT_EQ(1, client->num_sent_client_hellos());
}

TEST_F(QuicCryptoServerStreamTest, MessageAfterHandshake) {
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  CompleteCryptoHandshake();
  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE));
  message_.set_tag(kCHLO);
  ConstructHandshakeMessage();
  stream_.ProcessData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoServerStreamTest, BadMessageType) {
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  message_.set_tag(kSHLO);
  ConstructHandshakeMessage();
  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_INVALID_CRYPTO_MESSAGE_TYPE));
  stream_.ProcessData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoServerStreamTest, WithoutCertificates) {
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  crypto_config_.SetProofSource(NULL);
  client_options_.dont_verify_certs = true;

  // Only 2 client hellos need to be sent in the no-certs case: one to get the
  // source-address token and the second to finish.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(stream_.encryption_established());
  EXPECT_TRUE(stream_.handshake_confirmed());
}

TEST_F(QuicCryptoServerStreamTest, ChannelID) {
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  client_options_.channel_id_enabled = true;
  // TODO(rtenneti): Enable testing of ProofVerifier.
  // CompleteCryptoHandshake verifies
  // stream_.crypto_negotiated_params().channel_id is correct.
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(stream_.encryption_established());
  EXPECT_TRUE(stream_.handshake_confirmed());
}

}  // namespace
}  // namespace test
}  // namespace net
