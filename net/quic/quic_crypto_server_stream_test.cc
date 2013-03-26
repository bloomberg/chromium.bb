// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_server_stream.h"

#include <map>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
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
  virtual void OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE {
    frame_ = frame;
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
        addr_(),
        connection_(new PacketSavingConnection(guid_, addr_, true)),
        session_(connection_, true),
        stream_(&session_) {
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

  void CompleteCryptoHandshake() {
    CryptoTestUtils::HandshakeWithFakeClient(connection_, &stream_);
  }

 protected:
  QuicGuid guid_;
  IPEndPoint addr_;
  PacketSavingConnection* connection_;
  TestSession session_;
  QuicCryptoServerStream stream_;
  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;
};

TEST_F(QuicCryptoServerStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_.handshake_complete());
}

TEST_F(QuicCryptoServerStreamTest, ConnectedAfterCHLO) {
  CompleteCryptoHandshake();
  EXPECT_TRUE(stream_.handshake_complete());
}

TEST_F(QuicCryptoServerStreamTest, MessageAfterHandshake) {
  CompleteCryptoHandshake();
  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE));
  message_.set_tag(kCHLO);
  ConstructHandshakeMessage();
  stream_.ProcessData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoServerStreamTest, BadMessageType) {
  message_.set_tag(kSHLO);
  ConstructHandshakeMessage();
  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_INVALID_CRYPTO_MESSAGE_TYPE));
  stream_.ProcessData(message_data_->data(), message_data_->length());
}

}  // namespace
}  // namespace test
}  // namespace net
