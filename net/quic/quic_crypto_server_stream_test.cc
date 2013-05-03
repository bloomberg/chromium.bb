// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_server_stream.h"

#include <map>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/aes_128_gcm_encrypter.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_server_config.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
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

class TestSession: public QuicSession {
 public:
  TestSession(QuicConnection* connection, bool is_server)
      : QuicSession(connection, is_server) {
  }

  MOCK_METHOD1(CreateIncomingReliableStream,
               ReliableQuicStream*(QuicStreamId id));
  MOCK_METHOD0(GetCryptoStream, QuicCryptoStream*());
  MOCK_METHOD0(CreateOutgoingReliableStream, ReliableQuicStream*());
};

class QuicCryptoServerStreamTest : public ::testing::Test {
 public:
  QuicCryptoServerStreamTest()
      : guid_(1),
        addr_(ParseIPLiteralToNumber("192.0.2.33", &ip_) ?
              ip_ : IPAddressNumber(), 1),
        connection_(new PacketSavingConnection(guid_, addr_, true)),
        session_(connection_, true),
        crypto_config_(QuicCryptoServerConfig::TESTING),
        stream_(config_, crypto_config_, &session_) {
    // We advance the clock initially because the default time is zero and the
    // strike register worries that we've just overflowed a uint32 time.
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(100000));
    // TODO(rtenneti): Enable testing of ProofSource.
    // crypto_config_.SetProofSource(CryptoTestUtils::ProofSourceForTesting());

    CryptoTestUtils::SetupCryptoServerConfigForTest(
        connection_->clock(), connection_->random_generator(), &config_,
        &crypto_config_);
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

  int CompleteCryptoHandshake() {
    return CryptoTestUtils::HandshakeWithFakeClient(connection_, &stream_);
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
};

TEST_F(QuicCryptoServerStreamTest, NotInitiallyConected) {
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  EXPECT_FALSE(stream_.encryption_established());
  EXPECT_FALSE(stream_.handshake_confirmed());
}

TEST_F(QuicCryptoServerStreamTest, ConnectedAfterCHLO) {
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  // CompleteCryptoHandshake returns the number of client hellos sent. This
  // test should send:
  //   * One to get a source-address token.
  //   * One to complete the handshake.
  // TODO(rtenneti): Until we set the crypto_config.SetProofVerifier to enable
  // ProofVerifier in CryptoTestUtils::HandshakeWithFakeClient, we would not
  // have sent the following client hello.
  //   * One to get the server's certificates
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(stream_.encryption_established());
  EXPECT_TRUE(stream_.handshake_confirmed());
}

TEST_F(QuicCryptoServerStreamTest, ZeroRTT) {
  if (!Aes128GcmEncrypter::IsSupported()) {
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
  client_conn->AdvanceTime(QuicTime::Delta::FromSeconds(1000000));
  server_conn->AdvanceTime(QuicTime::Delta::FromSeconds(1000000));

  scoped_ptr<TestSession> client_session(new TestSession(client_conn, true));
  scoped_ptr<TestSession> server_session(new TestSession(server_conn, true));

  QuicConfig client_config;
  QuicCryptoClientConfig client_crypto_config;

  client_config.SetDefaults();
  client_crypto_config.SetDefaults();

  scoped_ptr<QuicCryptoClientStream> client(new QuicCryptoClientStream(
        "test.example.com", client_config, client_session.get(),
        &client_crypto_config));

  // Do a first handshake in order to prime the client config with the server's
  // information.
  CHECK(client->CryptoConnect());
  CHECK_EQ(1u, client_conn->packets_.size());

  scoped_ptr<QuicCryptoServerStream> server(
      new QuicCryptoServerStream(config_, crypto_config_,
                                 server_session.get()));

  CryptoTestUtils::CommunicateHandshakeMessages(
      client_conn, client.get(), server_conn, server.get());
  EXPECT_EQ(2, client->num_sent_client_hellos());

  // Now do another handshake, hopefully in 0-RTT.
  LOG(INFO) << "Resetting for 0-RTT handshake attempt";

  client_conn = new PacketSavingConnection(guid, addr, false);
  server_conn = new PacketSavingConnection(guid, addr, false);
  // We need to advance time past the strike-server window so that it's
  // authoritative in this time span.
  client_conn->AdvanceTime(QuicTime::Delta::FromSeconds(1002000));
  server_conn->AdvanceTime(QuicTime::Delta::FromSeconds(1002000));

  // This causes the client's nonce to be different and thus stops the
  // strike-register from rejecting the repeated nonce.
  client_conn->random_generator()->Reseed(NULL, 0);
  client_session.reset(new TestSession(client_conn, true));
  server_session.reset(new TestSession(server_conn, true));
  client.reset(new QuicCryptoClientStream(
        "test.example.com", client_config, client_session.get(),
        &client_crypto_config));
  server.reset(new QuicCryptoServerStream(config_, crypto_config_,
                                          server_session.get()));

  CHECK(client->CryptoConnect());

  CryptoTestUtils::CommunicateHandshakeMessages(
      client_conn, client.get(), server_conn, server.get());
  EXPECT_EQ(1, client->num_sent_client_hellos());
}

TEST_F(QuicCryptoServerStreamTest, MessageAfterHandshake) {
  if (!Aes128GcmEncrypter::IsSupported()) {
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
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  message_.set_tag(kSHLO);
  ConstructHandshakeMessage();
  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_INVALID_CRYPTO_MESSAGE_TYPE));
  stream_.ProcessData(message_data_->data(), message_data_->length());
}

}  // namespace
}  // namespace test
}  // namespace net
