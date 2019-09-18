// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/cast_framer.h"

#include <stdlib.h>
#include <string.h>

#include <limits>

#include "cast/sender/channel/proto/cast_channel.pb.h"
#include "platform/api/logging.h"
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

MessageFramer::MessageFramer(absl::Span<uint8_t> input_buffer)
    : input_buffer_(input_buffer) {}

MessageFramer::~MessageFramer() = default;

// static
ErrorOr<std::string> MessageFramer::Serialize(const CastMessage& message) {
  const size_t message_size = message.ByteSizeLong();
  if (message_size > kMaxBodySize || message_size == 0) {
    return ChannelError::kCastV2InvalidMessage;
  }
  std::string out(message_size + kHeaderSize, 0);
  openscreen::WriteBigEndian<uint32_t>(message_size, openscreen::data(out));
  if (!message.SerializeToArray(&out[kHeaderSize], message_size)) {
    return ChannelError::kCastV2InvalidMessage;
  }
  return out;
}

ErrorOr<size_t> MessageFramer::BytesRequested() const {
  if (message_bytes_received_ < kHeaderSize) {
    return kHeaderSize - message_bytes_received_;
  }

  const uint32_t message_size =
      openscreen::ReadBigEndian<uint32_t>(input_buffer_.data());
  if (message_size > kMaxBodySize) {
    return ChannelError::kCastV2InvalidMessage;
  }
  return (kHeaderSize + message_size) - message_bytes_received_;
}

ErrorOr<CastMessage> MessageFramer::TryDeserialize(size_t byte_count) {
  message_bytes_received_ += byte_count;
  if (message_bytes_received_ > input_buffer_.size()) {
    return ChannelError::kCastV2InvalidMessage;
  }

  if (message_bytes_received_ < kHeaderSize) {
    return ChannelError::kInsufficientBuffer;
  }

  const uint32_t message_size =
      openscreen::ReadBigEndian<uint32_t>(input_buffer_.data());
  if (message_size > kMaxBodySize) {
    return ChannelError::kCastV2InvalidMessage;
  }

  if (message_bytes_received_ < (kHeaderSize + message_size)) {
    return ChannelError::kInsufficientBuffer;
  }

  CastMessage parsed_message;
  if (!parsed_message.ParseFromArray(input_buffer_.data() + kHeaderSize,
                                     message_size)) {
    return ChannelError::kCastV2InvalidMessage;
  }

  message_bytes_received_ = 0;
  return parsed_message;
}

}  // namespace channel
}  // namespace cast
