// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/cast_framer.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "cast/sender/channel/proto/cast_channel.pb.h"
#include "gtest/gtest.h"
#include "util/big_endian.h"
#include "util/std_util.h"

namespace cast {
namespace channel {

using ChannelError = openscreen::Error::Code;

namespace {

static constexpr size_t kHeaderSize = sizeof(uint32_t);

// Cast specifies a max message body size of 64 KiB.
static constexpr size_t kMaxBodySize = 65536;

}  // namespace

class CastFramerTest : public testing::Test {
 public:
  CastFramerTest()
      : buffer_(kHeaderSize + kMaxBodySize),
        framer_(absl::Span<uint8_t>(&buffer_[0], buffer_.size())) {}

  void SetUp() override {
    cast_message_.set_protocol_version(CastMessage::CASTV2_1_0);
    cast_message_.set_source_id("source");
    cast_message_.set_destination_id("destination");
    cast_message_.set_namespace_("namespace");
    cast_message_.set_payload_type(CastMessage::STRING);
    cast_message_.set_payload_utf8("payload");
    ErrorOr<std::string> result = MessageFramer::Serialize(cast_message_);
    ASSERT_TRUE(result.is_value());
    cast_message_str_ = std::move(result.value());
  }

  void WriteToBuffer(const std::string& data) {
    memcpy(&buffer_[0], data.data(), data.size());
  }

 protected:
  CastMessage cast_message_;
  std::string cast_message_str_;
  std::vector<uint8_t> buffer_;
  MessageFramer framer_;
};

TEST_F(CastFramerTest, TestMessageFramerCompleteMessage) {
  WriteToBuffer(cast_message_str_);

  // Receive 1 byte of the header, framer demands 3 more bytes.
  EXPECT_EQ(4u, framer_.BytesRequested().value());
  ErrorOr<CastMessage> result = framer_.TryDeserialize(1);
  EXPECT_FALSE(result);
  EXPECT_EQ(ChannelError::kInsufficientBuffer, result.error().code());
  EXPECT_EQ(3u, framer_.BytesRequested().value());

  // TryDeserialize remaining 3, expect that the framer has moved on to
  // requesting the body contents.
  result = framer_.TryDeserialize(3);
  EXPECT_FALSE(result);
  EXPECT_EQ(ChannelError::kInsufficientBuffer, result.error().code());
  EXPECT_EQ(cast_message_str_.size() - kHeaderSize,
            framer_.BytesRequested().value());

  // Remainder of packet sent over the wire.
  result = framer_.TryDeserialize(framer_.BytesRequested().value());
  ASSERT_TRUE(result);
  const CastMessage& message = result.value();
  EXPECT_EQ(message.SerializeAsString(), cast_message_.SerializeAsString());
  EXPECT_EQ(4u, framer_.BytesRequested().value());
}

TEST_F(CastFramerTest, BigEndianMessageHeader) {
  WriteToBuffer(cast_message_str_);

  EXPECT_EQ(4u, framer_.BytesRequested().value());
  ErrorOr<CastMessage> result = framer_.TryDeserialize(4);
  EXPECT_FALSE(result);
  EXPECT_EQ(ChannelError::kInsufficientBuffer, result.error().code());

  const uint32_t expected_size =
      openscreen::ReadBigEndian<uint32_t>(openscreen::data(cast_message_str_));
  EXPECT_EQ(expected_size, framer_.BytesRequested().value());
}

TEST_F(CastFramerTest, TestSerializeErrorMessageTooLarge) {
  CastMessage big_message;
  big_message.CopyFrom(cast_message_);
  std::string payload;
  payload.append(kMaxBodySize + 1, 'x');
  big_message.set_payload_utf8(payload);
  EXPECT_FALSE(MessageFramer::Serialize(big_message));
}

TEST_F(CastFramerTest, TestCompleteMessageAtOnce) {
  WriteToBuffer(cast_message_str_);

  ErrorOr<CastMessage> result =
      framer_.TryDeserialize(cast_message_str_.size());
  ASSERT_TRUE(result);
  const CastMessage& message = result.value();
  EXPECT_EQ(message.SerializeAsString(), cast_message_.SerializeAsString());
  EXPECT_EQ(4u, framer_.BytesRequested().value());
}

TEST_F(CastFramerTest, TestTryDeserializeIllegalLargeMessage) {
  std::string mangled_cast_message = cast_message_str_;
  mangled_cast_message[0] = 88;
  mangled_cast_message[1] = 88;
  mangled_cast_message[2] = 88;
  mangled_cast_message[3] = 88;
  WriteToBuffer(mangled_cast_message);

  EXPECT_EQ(4u, framer_.BytesRequested().value());
  ErrorOr<CastMessage> result = framer_.TryDeserialize(4);
  ASSERT_FALSE(result);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage, result.error().code());
  ErrorOr<size_t> bytes_requested = framer_.BytesRequested();
  ASSERT_FALSE(bytes_requested);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage,
            bytes_requested.error().code());
}

TEST_F(CastFramerTest, TestTryDeserializeIllegalLargeMessage2) {
  std::string mangled_cast_message = cast_message_str_;
  // Header indicates body size is 0x00010001 = 65537
  mangled_cast_message[0] = 0;
  mangled_cast_message[1] = 0x1;
  mangled_cast_message[2] = 0;
  mangled_cast_message[3] = 0x1;
  WriteToBuffer(mangled_cast_message);

  EXPECT_EQ(4u, framer_.BytesRequested().value());
  ErrorOr<CastMessage> result = framer_.TryDeserialize(4);
  ASSERT_FALSE(result);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage, result.error().code());
  ErrorOr<size_t> bytes_requested = framer_.BytesRequested();
  ASSERT_FALSE(bytes_requested);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage,
            bytes_requested.error().code());
}

TEST_F(CastFramerTest, TestUnparsableBodyProto) {
  // Message header is OK, but the body is replaced with "x"es.
  std::string mangled_cast_message = cast_message_str_;
  for (size_t i = kHeaderSize; i < mangled_cast_message.size(); ++i) {
    std::fill(mangled_cast_message.begin() + kHeaderSize,
              mangled_cast_message.end(), 'x');
  }
  WriteToBuffer(mangled_cast_message);

  // Send header.
  EXPECT_EQ(4u, framer_.BytesRequested().value());
  ErrorOr<CastMessage> result = framer_.TryDeserialize(4);
  EXPECT_FALSE(result);
  EXPECT_EQ(ChannelError::kInsufficientBuffer, result.error().code());
  EXPECT_EQ(cast_message_str_.size() - 4, framer_.BytesRequested().value());

  // Send body, expect an error.
  result = framer_.TryDeserialize(framer_.BytesRequested().value());
  ASSERT_FALSE(result);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage, result.error().code());
}

}  // namespace channel
}  // namespace cast
