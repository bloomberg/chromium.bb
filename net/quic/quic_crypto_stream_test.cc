// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_stream.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using std::vector;

namespace net {
namespace test {
namespace {

class MockQuicCryptoStream : public QuicCryptoStream {
 public:
  explicit MockQuicCryptoStream(QuicSession* session)
      : QuicCryptoStream(session) {
  }

  void OnHandshakeMessage(const CryptoHandshakeMessage& message) {
    messages_.push_back(message);
  }

  vector<CryptoHandshakeMessage>* messages() {
    return &messages_;
  }

 private:
  vector<CryptoHandshakeMessage> messages_;

  DISALLOW_COPY_AND_ASSIGN(MockQuicCryptoStream);
};

class QuicCryptoStreamTest : public ::testing::Test {
 public:
  QuicCryptoStreamTest()
      : addr_(IPAddressNumber(), 1),
        connection_(new MockConnection(1, addr_)),
        session_(connection_, true),
        stream_(&session_) {
    message_.tag = kSHLO;
    message_.tag_value_map[1] = "abc";
    message_.tag_value_map[2] = "def";
    ConstructHandshakeMessage();
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

 protected:
  IPEndPoint addr_;
  MockConnection* connection_;
  MockSession session_;
  MockQuicCryptoStream stream_;
  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicCryptoStreamTest);
};

TEST_F(QuicCryptoStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_.handshake_complete());
}

TEST_F(QuicCryptoStreamTest, OnErrorClosesConnection) {
  CryptoFramer framer;
  EXPECT_CALL(session_, ConnectionClose(QUIC_NO_ERROR, false));
  stream_.OnError(&framer);
}

TEST_F(QuicCryptoStreamTest, ProcessData) {
  EXPECT_EQ(message_data_->length(),
            stream_.ProcessData(message_data_->data(),
                                message_data_->length()));
  ASSERT_EQ(1u, stream_.messages()->size());
  EXPECT_EQ(kSHLO, (*stream_.messages())[0].tag);
  EXPECT_EQ(2u, (*stream_.messages())[0].tag_value_map.size());
  EXPECT_EQ("abc", (*stream_.messages())[0].tag_value_map[1]);
  EXPECT_EQ("def", (*stream_.messages())[0].tag_value_map[2]);
}

TEST_F(QuicCryptoStreamTest, ProcessBadData) {
  string bad(message_data_->data(), message_data_->length());
  bad[6] = 0x7F;  // out of order tag

  EXPECT_CALL(*connection_,
              SendConnectionClose(QUIC_CRYPTO_TAGS_OUT_OF_ORDER));
  EXPECT_EQ(0u, stream_.ProcessData(bad.data(), bad.length()));
}

}  // namespace
}  // namespace test
}  // namespace net
