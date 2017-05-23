// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/message_payload_parser.h"

#include "base/big_endian.h"

namespace gcm {

namespace {

// Size, in bytes, of the salt included in the message header.
constexpr size_t kSaltSize = 16;

// Size, in bytes, of the uncompressed point included in the message header.
constexpr size_t kUncompressedPointSize = 65;

// Size, in bytes, of the smallest allowable record_size value.
constexpr size_t kMinimumRecordSize = 18;

// Size, in bytes, of an empty message with the minimum amount of padding.
constexpr size_t kMinimumMessageSize =
    kSaltSize + sizeof(uint32_t) + sizeof(uint8_t) + kUncompressedPointSize +
    kMinimumRecordSize;

}  // namespace

MessagePayloadParser::MessagePayloadParser(base::StringPiece message) {
  if (message.size() < kMinimumMessageSize)
    return;

  salt_ = message.substr(0, kSaltSize).as_string();
  message.remove_prefix(kSaltSize);

  base::ReadBigEndian(message.data(), &record_size_);
  message.remove_prefix(sizeof(record_size_));

  if (record_size_ < kMinimumRecordSize)
    return;

  uint8_t public_key_length;
  base::ReadBigEndian(message.data(), &public_key_length);
  message.remove_prefix(sizeof(public_key_length));

  if (public_key_length != kUncompressedPointSize)
    return;

  if (message[0] != 0x04)
    return;

  public_key_ = message.substr(0, kUncompressedPointSize).as_string();
  message.remove_prefix(kUncompressedPointSize);

  ciphertext_ = message.as_string();

  if (ciphertext_.size() < kMinimumRecordSize)
    return;

  is_valid_ = true;
}

MessagePayloadParser::~MessagePayloadParser() = default;

}  // namespace gcm
