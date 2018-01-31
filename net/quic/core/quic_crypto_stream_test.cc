// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_crypto_stream.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "net/quic/core/crypto/crypto_handshake.h"
#include "net/quic/core/crypto/crypto_protocol.h"
#include "net/quic/platform/api/quic_socket_address.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_stream_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"

using std::string;

using testing::_;
using testing::InSequence;
using testing::Invoke;

namespace net {
namespace test {
namespace {

class MockQuicCryptoStream : public QuicCryptoStream,
                             public QuicCryptoHandshaker {
 public:
  explicit MockQuicCryptoStream(QuicSession* session)
      : QuicCryptoStream(session),
        QuicCryptoHandshaker(this, session),
        params_(new QuicCryptoNegotiatedParameters) {}

  void OnHandshakeMessage(const CryptoHandshakeMessage& message) override {
    messages_.push_back(message);
  }

  std::vector<CryptoHandshakeMessage>* messages() { return &messages_; }

  bool encryption_established() const override { return false; }
  bool handshake_confirmed() const override { return false; }

  const QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override {
    return *params_;
  }
  CryptoMessageParser* crypto_message_parser() override {
    return QuicCryptoHandshaker::crypto_message_parser();
  }

 private:
  QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters> params_;
  std::vector<CryptoHandshakeMessage> messages_;

  DISALLOW_COPY_AND_ASSIGN(MockQuicCryptoStream);
};

class QuicCryptoStreamTest : public QuicTest {
 public:
  QuicCryptoStreamTest()
      : connection_(new MockQuicConnection(&helper_,
                                           &alarm_factory_,
                                           Perspective::IS_CLIENT)),
        session_(connection_),
        stream_(&session_) {
    message_.set_tag(kSHLO);
    message_.SetStringPiece(1, "abc");
    message_.SetStringPiece(2, "def");
    ConstructHandshakeMessage(Perspective::IS_SERVER);
  }

  void ConstructHandshakeMessage(Perspective perspective) {
    CryptoFramer framer;
    message_data_.reset(
        framer.ConstructHandshakeMessage(message_, perspective));
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* connection_;
  MockQuicSpdySession session_;
  MockQuicCryptoStream stream_;
  CryptoHandshakeMessage message_;
  std::unique_ptr<QuicData> message_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicCryptoStreamTest);
};

TEST_F(QuicCryptoStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_.encryption_established());
  EXPECT_FALSE(stream_.handshake_confirmed());
}

TEST_F(QuicCryptoStreamTest, ProcessRawData) {
  stream_.OnStreamFrame(QuicStreamFrame(kCryptoStreamId, /*fin=*/false,
                                        /*offset=*/0,
                                        message_data_->AsStringPiece()));
  ASSERT_EQ(1u, stream_.messages()->size());
  const CryptoHandshakeMessage& message = (*stream_.messages())[0];
  EXPECT_EQ(kSHLO, message.tag());
  EXPECT_EQ(2u, message.tag_value_map().size());
  EXPECT_EQ("abc", crypto_test_utils::GetValueForTag(message, 1));
  EXPECT_EQ("def", crypto_test_utils::GetValueForTag(message, 2));
}

TEST_F(QuicCryptoStreamTest, ProcessBadData) {
  string bad(message_data_->data(), message_data_->length());
  const int kFirstTagIndex = sizeof(uint32_t) +  // message tag
                             sizeof(uint16_t) +  // number of tag-value pairs
                             sizeof(uint16_t);   // padding
  EXPECT_EQ(1, bad[kFirstTagIndex]);
  bad[kFirstTagIndex] = 0x7F;  // out of order tag

  EXPECT_CALL(*connection_, CloseConnection(QUIC_CRYPTO_TAGS_OUT_OF_ORDER,
                                            testing::_, testing::_));
  stream_.OnStreamFrame(
      QuicStreamFrame(kCryptoStreamId, /*fin=*/false, /*offset=*/0, bad));
}

TEST_F(QuicCryptoStreamTest, NoConnectionLevelFlowControl) {
  EXPECT_FALSE(
      QuicStreamPeer::StreamContributesToConnectionFlowControl(&stream_));
}

TEST_F(QuicCryptoStreamTest, RetransmitCryptoData) {
  InSequence s;
  // Send [0, 1350) in ENCRYPTION_NONE.
  EXPECT_EQ(ENCRYPTION_NONE, connection_->encryption_level());
  string data(1350, 'a');
  EXPECT_CALL(session_, WritevData(_, kCryptoStreamId, 1350, 0, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_.WriteOrBufferData(data, false, nullptr);
  // Send [1350, 2700) in ENCRYPTION_INITIAL.
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  EXPECT_CALL(session_, WritevData(_, kCryptoStreamId, 1350, 1350, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_.WriteOrBufferData(data, false, nullptr);
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  // Lost [0, 1000).
  stream_.OnStreamFrameLost(0, 1000, false);
  EXPECT_TRUE(stream_.HasPendingRetransmission());
  // Lost [1200, 2000).
  stream_.OnStreamFrameLost(1200, 800, false);
  EXPECT_CALL(session_, WritevData(_, kCryptoStreamId, 1000, 0, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  // Verify [1200, 2000) are sent in [1200, 1350) and [1350, 2000) because of
  // they are in different encryption levels.
  EXPECT_CALL(session_, WritevData(_, kCryptoStreamId, 150, 1200, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  EXPECT_CALL(session_, WritevData(_, kCryptoStreamId, 650, 1350, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_.OnCanWrite();
  EXPECT_FALSE(stream_.HasPendingRetransmission());
  // Verify connection's encryption level has restored.
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());
}

TEST_F(QuicCryptoStreamTest, NeuterUnencryptedStreamData) {
  // Send [0, 1350) in ENCRYPTION_NONE.
  EXPECT_EQ(ENCRYPTION_NONE, connection_->encryption_level());
  string data(1350, 'a');
  EXPECT_CALL(session_, WritevData(_, kCryptoStreamId, 1350, 0, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_.WriteOrBufferData(data, false, nullptr);
  // Send [1350, 2700) in ENCRYPTION_INITIAL.
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  EXPECT_CALL(session_, WritevData(_, kCryptoStreamId, 1350, 1350, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_.WriteOrBufferData(data, false, nullptr);

  // Lost [0, 1350).
  stream_.OnStreamFrameLost(0, 1350, false);
  EXPECT_TRUE(stream_.HasPendingRetransmission());
  // Neuters [0, 1350).
  stream_.NeuterUnencryptedStreamData();
  EXPECT_FALSE(stream_.HasPendingRetransmission());
  // Lost [0, 1350) again.
  stream_.OnStreamFrameLost(0, 1350, false);
  EXPECT_FALSE(stream_.HasPendingRetransmission());

  // Lost [1350, 2000).
  stream_.OnStreamFrameLost(1350, 650, false);
  EXPECT_TRUE(stream_.HasPendingRetransmission());
  stream_.NeuterUnencryptedStreamData();
  EXPECT_TRUE(stream_.HasPendingRetransmission());
}

}  // namespace
}  // namespace test
}  // namespace net
