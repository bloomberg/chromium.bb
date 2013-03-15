// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_client_stream.h"

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

const char kServerHostname[] = "localhost";

class TestQuicVisitor : public NoOpFramerVisitor {
 public:
  TestQuicVisitor()
      : frame_valid_(false) {
  }

  // NoOpFramerVisitor
  virtual void OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE {
    frame_ = frame;
    frame_valid_ = true;
  }

  bool frame_valid() const {
    return frame_valid_;
  }
  QuicStreamFrame* frame() { return &frame_; }

 private:
  QuicStreamFrame frame_;
  bool frame_valid_;

  DISALLOW_COPY_AND_ASSIGN(TestQuicVisitor);
};

// The same as MockSession, except that WriteData() is not mocked.
class TestMockSession : public MockSession {
 public:
  TestMockSession(QuicConnection* connection, bool is_server)
      : MockSession(connection, is_server) {
  }
  virtual ~TestMockSession() {}

  virtual QuicConsumedData WriteData(QuicStreamId id,
                                     base::StringPiece data,
                                     QuicStreamOffset offset,
                                     bool fin) OVERRIDE {
    return QuicSession::WriteData(id, data, offset, fin);
  }
};

class QuicCryptoClientStreamTest : public ::testing::Test {
 public:
  QuicCryptoClientStreamTest()
      : addr_(),
        connection_(new PacketSavingConnection(1, addr_, true)),
        session_(connection_, true),
        stream_(&session_, kServerHostname) {
  }

  void CompleteCryptoHandshake() {
    EXPECT_TRUE(stream_.CryptoConnect());
    CryptoTestUtils::HandshakeWithFakeServer(connection_, &stream_);
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

  IPEndPoint addr_;
  PacketSavingConnection* connection_;
  TestMockSession session_;
  QuicCryptoClientStream stream_;
  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;
};

TEST_F(QuicCryptoClientStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_.handshake_complete());
}

TEST_F(QuicCryptoClientStreamTest, ClientHelloContents) {
  EXPECT_TRUE(stream_.CryptoConnect());

  SimpleQuicFramer framer;
  ASSERT_TRUE(framer.ProcessPacket(*connection_->packets_[0]));
  ASSERT_EQ(1u, framer.stream_frames().size());
  const QuicStreamFrame& frame(framer.stream_frames()[0]);
  EXPECT_EQ(kCryptoStreamId, frame.stream_id);
  EXPECT_FALSE(frame.fin);
  EXPECT_EQ(0u, frame.offset);

  scoped_ptr<CryptoHandshakeMessage> chlo(framer.HandshakeMessage(0));
  EXPECT_EQ(kCHLO, chlo->tag);

  CryptoTagValueMap& tag_value_map = chlo->tag_value_map;

  // kSNI
  EXPECT_EQ(kServerHostname, tag_value_map[kSNI]);

  // kNONC
  // TODO(wtc): check the nonce.
  ASSERT_EQ(32u, tag_value_map[kNONC].size());

  // kVERS
  ASSERT_EQ(2u, tag_value_map[kVERS].size());
  uint16 version;
  memcpy(&version, tag_value_map[kVERS].data(), 2);
  EXPECT_EQ(0u, version);

  // kKEXS
  ASSERT_EQ(4u, tag_value_map[kKEXS].size());
  CryptoTag key_exchange[1];
  memcpy(&key_exchange[0], &tag_value_map[kKEXS][0], 4);
  EXPECT_EQ(kC255, key_exchange[0]);

  // kAEAD
  ASSERT_EQ(4u, tag_value_map[kAEAD].size());
  CryptoTag cipher[1];
  memcpy(&cipher[0], &tag_value_map[kAEAD][0], 4);
  EXPECT_EQ(kAESG, cipher[0]);

  // kICSL
  ASSERT_EQ(4u, tag_value_map[kICSL].size());
  uint32 idle_lifetime;
  memcpy(&idle_lifetime, tag_value_map[kICSL].data(), 4);
  EXPECT_EQ(300u, idle_lifetime);

  // kKATO
  ASSERT_EQ(4u, tag_value_map[kKATO].size());
  uint32 keepalive_timeout;
  memcpy(&keepalive_timeout, tag_value_map[kKATO].data(), 4);
  EXPECT_EQ(0u, keepalive_timeout);

  // kCGST
  ASSERT_EQ(4u, tag_value_map[kCGST].size());
  CryptoTag congestion[1];
  memcpy(&congestion[0], &tag_value_map[kCGST][0], 4);
  EXPECT_EQ(kQBIC, congestion[0]);
}

TEST_F(QuicCryptoClientStreamTest, ConnectedAfterSHLO) {
  CompleteCryptoHandshake();
  EXPECT_TRUE(stream_.handshake_complete());
}

TEST_F(QuicCryptoClientStreamTest, MessageAfterHandshake) {
  CompleteCryptoHandshake();

  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE));
  message_.tag = kCHLO;
  ConstructHandshakeMessage();
  stream_.ProcessData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoClientStreamTest, BadMessageType) {
  message_.tag = kCHLO;
  ConstructHandshakeMessage();

  EXPECT_CALL(*connection_,
              SendConnectionClose(QUIC_INVALID_CRYPTO_MESSAGE_TYPE));
  stream_.ProcessData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoClientStreamTest, CryptoConnect) {
  EXPECT_TRUE(stream_.CryptoConnect());
}

}  // namespace
}  // namespace test
}  // namespace net
