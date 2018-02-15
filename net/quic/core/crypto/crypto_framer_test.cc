// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/crypto/crypto_framer.h"

#include <map>
#include <memory>
#include <vector>

#include "net/quic/core/crypto/crypto_handshake.h"
#include "net/quic/core/crypto/crypto_protocol.h"
#include "net/quic/core/quic_packets.h"
#include "net/quic/platform/api/quic_arraysize.h"
#include "net/quic/platform/api/quic_logging.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"

using std::string;

namespace net {
namespace test {
namespace {

char* AsChars(unsigned char* data) {
  return reinterpret_cast<char*>(data);
}

class CryptoFramerTest : public QuicTestWithParam<Perspective> {};

class TestCryptoVisitor : public CryptoFramerVisitorInterface {
 public:
  TestCryptoVisitor() : error_count_(0) {}

  void OnError(CryptoFramer* framer) override {
    QUIC_DLOG(ERROR) << "CryptoFramer Error: " << framer->error();
    ++error_count_;
  }

  void OnHandshakeMessage(const CryptoHandshakeMessage& message) override {
    messages_.push_back(message);
  }

  // Counters from the visitor callbacks.
  int error_count_;

  std::vector<CryptoHandshakeMessage> messages_;
};

INSTANTIATE_TEST_CASE_P(Tests,
                        CryptoFramerTest,
                        ::testing::ValuesIn({Perspective::IS_CLIENT,
                                             Perspective::IS_SERVER}));

TEST_P(CryptoFramerTest, ConstructHandshakeMessage) {
  CryptoHandshakeMessage message;
  message.set_tag(0xFFAA7733);
  message.SetStringPiece(0x12345678, "abcdef");
  message.SetStringPiece(0x12345679, "ghijk");
  message.SetStringPiece(0x1234567A, "lmnopqr");

  unsigned char packet[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x03, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x78, 0x56, 0x34, 0x12,
      // end offset 1
      0x06, 0x00, 0x00, 0x00,
      // tag 2
      0x79, 0x56, 0x34, 0x12,
      // end offset 2
      0x0b, 0x00, 0x00, 0x00,
      // tag 3
      0x7A, 0x56, 0x34, 0x12,
      // end offset 3
      0x12, 0x00, 0x00, 0x00,
      // value 1
      'a', 'b', 'c', 'd', 'e', 'f',
      // value 2
      'g', 'h', 'i', 'j', 'k',
      // value 3
      'l', 'm', 'n', 'o', 'p', 'q', 'r',
  };

  CryptoFramer framer;
  std::unique_ptr<QuicData> data(
      framer.ConstructHandshakeMessage(message, GetParam()));
  ASSERT_TRUE(data != nullptr);
  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(CryptoFramerTest, ConstructHandshakeMessageWithTwoKeys) {
  CryptoHandshakeMessage message;
  message.set_tag(0xFFAA7733);
  message.SetStringPiece(0x12345678, "abcdef");
  message.SetStringPiece(0x12345679, "ghijk");

  unsigned char packet[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x02, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x78, 0x56, 0x34, 0x12,
      // end offset 1
      0x06, 0x00, 0x00, 0x00,
      // tag 2
      0x79, 0x56, 0x34, 0x12,
      // end offset 2
      0x0b, 0x00, 0x00, 0x00,
      // value 1
      'a', 'b', 'c', 'd', 'e', 'f',
      // value 2
      'g', 'h', 'i', 'j', 'k',
  };

  CryptoFramer framer;
  std::unique_ptr<QuicData> data(
      framer.ConstructHandshakeMessage(message, GetParam()));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(CryptoFramerTest, ConstructHandshakeMessageZeroLength) {
  CryptoHandshakeMessage message;
  message.set_tag(0xFFAA7733);
  message.SetStringPiece(0x12345678, "");

  unsigned char packet[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x01, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x78, 0x56, 0x34, 0x12,
      // end offset 1
      0x00, 0x00, 0x00, 0x00,
  };

  CryptoFramer framer;
  std::unique_ptr<QuicData> data(
      framer.ConstructHandshakeMessage(message, GetParam()));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(CryptoFramerTest, ConstructHandshakeMessageTooManyEntries) {
  CryptoHandshakeMessage message;
  message.set_tag(0xFFAA7733);
  for (uint32_t key = 1; key <= kMaxEntries + 1; ++key) {
    message.SetStringPiece(key, "abcdef");
  }

  CryptoFramer framer;
  std::unique_ptr<QuicData> data(
      framer.ConstructHandshakeMessage(message, GetParam()));
  EXPECT_TRUE(data == nullptr);
}

TEST_P(CryptoFramerTest, ConstructHandshakeMessageMinimumSize) {
  CryptoHandshakeMessage message;
  message.set_tag(0xFFAA7733);
  message.SetStringPiece(0x01020304, "test");
  message.set_minimum_size(64);

  unsigned char packet[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x02, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      'P', 'A', 'D', 0,
      // end offset 1
      0x24, 0x00, 0x00, 0x00,
      // tag 2
      0x04, 0x03, 0x02, 0x01,
      // end offset 2
      0x28, 0x00, 0x00, 0x00,
      // 36 bytes of padding.
      '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
      '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
      '-', '-', '-', '-', '-', '-',
      // value 2
      't', 'e', 's', 't',
  };

  CryptoFramer framer;
  std::unique_ptr<QuicData> data(
      framer.ConstructHandshakeMessage(message, GetParam()));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(CryptoFramerTest, ConstructHandshakeMessageMinimumSizePadLast) {
  CryptoHandshakeMessage message;
  message.set_tag(0xFFAA7733);
  message.SetStringPiece(1, "");
  message.set_minimum_size(64);

  unsigned char packet[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x02, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x01, 0x00, 0x00, 0x00,
      // end offset 1
      0x00, 0x00, 0x00, 0x00,
      // tag 2
      'P', 'A', 'D', 0,
      // end offset 2
      0x28, 0x00, 0x00, 0x00,
      // 40 bytes of padding.
      '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
      '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
      '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
  };

  CryptoFramer framer;
  std::unique_ptr<QuicData> data(
      framer.ConstructHandshakeMessage(message, GetParam()));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      QUIC_ARRAYSIZE(packet));
}

TEST_P(CryptoFramerTest, ProcessInput) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x02, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x78, 0x56, 0x34, 0x12,
      // end offset 1
      0x06, 0x00, 0x00, 0x00,
      // tag 2
      0x79, 0x56, 0x34, 0x12,
      // end offset 2
      0x0b, 0x00, 0x00, 0x00,
      // value 1
      'a', 'b', 'c', 'd', 'e', 'f',
      // value 2
      'g', 'h', 'i', 'j', 'k',
  };

  EXPECT_TRUE(framer.ProcessInput(
      QuicStringPiece(AsChars(input), QUIC_ARRAYSIZE(input)), GetParam()));
  EXPECT_EQ(0u, framer.InputBytesRemaining());
  EXPECT_EQ(0, visitor.error_count_);
  ASSERT_EQ(1u, visitor.messages_.size());
  const CryptoHandshakeMessage& message = visitor.messages_[0];
  EXPECT_EQ(0xFFAA7733, message.tag());
  EXPECT_EQ(2u, message.tag_value_map().size());
  EXPECT_EQ("abcdef", crypto_test_utils::GetValueForTag(message, 0x12345678));
  EXPECT_EQ("ghijk", crypto_test_utils::GetValueForTag(message, 0x12345679));
}

TEST_P(CryptoFramerTest, ProcessInputWithThreeKeys) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x03, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x78, 0x56, 0x34, 0x12,
      // end offset 1
      0x06, 0x00, 0x00, 0x00,
      // tag 2
      0x79, 0x56, 0x34, 0x12,
      // end offset 2
      0x0b, 0x00, 0x00, 0x00,
      // tag 3
      0x7A, 0x56, 0x34, 0x12,
      // end offset 3
      0x12, 0x00, 0x00, 0x00,
      // value 1
      'a', 'b', 'c', 'd', 'e', 'f',
      // value 2
      'g', 'h', 'i', 'j', 'k',
      // value 3
      'l', 'm', 'n', 'o', 'p', 'q', 'r',
  };

  EXPECT_TRUE(framer.ProcessInput(
      QuicStringPiece(AsChars(input), QUIC_ARRAYSIZE(input)), GetParam()));
  EXPECT_EQ(0u, framer.InputBytesRemaining());
  EXPECT_EQ(0, visitor.error_count_);
  ASSERT_EQ(1u, visitor.messages_.size());
  const CryptoHandshakeMessage& message = visitor.messages_[0];
  EXPECT_EQ(0xFFAA7733, message.tag());
  EXPECT_EQ(3u, message.tag_value_map().size());
  EXPECT_EQ("abcdef", crypto_test_utils::GetValueForTag(message, 0x12345678));
  EXPECT_EQ("ghijk", crypto_test_utils::GetValueForTag(message, 0x12345679));
  EXPECT_EQ("lmnopqr", crypto_test_utils::GetValueForTag(message, 0x1234567A));
}

TEST_P(CryptoFramerTest, ProcessInputIncrementally) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x02, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x78, 0x56, 0x34, 0x12,
      // end offset 1
      0x06, 0x00, 0x00, 0x00,
      // tag 2
      0x79, 0x56, 0x34, 0x12,
      // end offset 2
      0x0b, 0x00, 0x00, 0x00,
      // value 1
      'a', 'b', 'c', 'd', 'e', 'f',
      // value 2
      'g', 'h', 'i', 'j', 'k',
  };

  for (size_t i = 0; i < QUIC_ARRAYSIZE(input); i++) {
    EXPECT_TRUE(framer.ProcessInput(QuicStringPiece(AsChars(input) + i, 1),
                                    GetParam()));
  }
  EXPECT_EQ(0u, framer.InputBytesRemaining());
  ASSERT_EQ(1u, visitor.messages_.size());
  const CryptoHandshakeMessage& message = visitor.messages_[0];
  EXPECT_EQ(0xFFAA7733, message.tag());
  EXPECT_EQ(2u, message.tag_value_map().size());
  EXPECT_EQ("abcdef", crypto_test_utils::GetValueForTag(message, 0x12345678));
  EXPECT_EQ("ghijk", crypto_test_utils::GetValueForTag(message, 0x12345679));
}

TEST_P(CryptoFramerTest, ProcessInputTagsOutOfOrder) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x02, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x78, 0x56, 0x34, 0x13,
      // end offset 1
      0x01, 0x00, 0x00, 0x00,
      // tag 2
      0x79, 0x56, 0x34, 0x12,
      // end offset 2
      0x02, 0x00, 0x00, 0x00,
  };

  EXPECT_FALSE(framer.ProcessInput(
      QuicStringPiece(AsChars(input), QUIC_ARRAYSIZE(input)), GetParam()));
  EXPECT_EQ(QUIC_CRYPTO_TAGS_OUT_OF_ORDER, framer.error());
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(CryptoFramerTest, ProcessEndOffsetsOutOfOrder) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x02, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x79, 0x56, 0x34, 0x12,
      // end offset 1
      0x01, 0x00, 0x00, 0x00,
      // tag 2
      0x78, 0x56, 0x34, 0x13,
      // end offset 2
      0x00, 0x00, 0x00, 0x00,
  };

  EXPECT_FALSE(framer.ProcessInput(
      QuicStringPiece(AsChars(input), QUIC_ARRAYSIZE(input)), GetParam()));
  EXPECT_EQ(QUIC_CRYPTO_TAGS_OUT_OF_ORDER, framer.error());
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(CryptoFramerTest, ProcessInputTooManyEntries) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0xA0, 0x00,
      // padding
      0x00, 0x00,
  };

  EXPECT_FALSE(framer.ProcessInput(
      QuicStringPiece(AsChars(input), QUIC_ARRAYSIZE(input)), GetParam()));
  EXPECT_EQ(QUIC_CRYPTO_TOO_MANY_ENTRIES, framer.error());
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(CryptoFramerTest, ProcessInputZeroLength) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
      // tag
      0x33, 0x77, 0xAA, 0xFF,
      // num entries
      0x02, 0x00,
      // padding
      0x00, 0x00,
      // tag 1
      0x78, 0x56, 0x34, 0x12,
      // end offset 1
      0x00, 0x00, 0x00, 0x00,
      // tag 2
      0x79, 0x56, 0x34, 0x12,
      // end offset 2
      0x05, 0x00, 0x00, 0x00,
  };

  EXPECT_TRUE(framer.ProcessInput(
      QuicStringPiece(AsChars(input), QUIC_ARRAYSIZE(input)), GetParam()));
  EXPECT_EQ(0, visitor.error_count_);
}

}  // namespace
}  // namespace test
}  // namespace net
