// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_input_stream.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace net {

using base::StringPiece;

HpackInputStream::HpackInputStream(uint32 max_string_literal_size,
                                   StringPiece buffer)
    : max_string_literal_size_(max_string_literal_size),
      buffer_(buffer),
      bit_offset_(0) {}

HpackInputStream::~HpackInputStream() {}

bool HpackInputStream::HasMoreData() const {
  return !buffer_.empty();
}

bool HpackInputStream::MatchPrefixAndConsume(HpackPrefix prefix) {
  DCHECK_GT(prefix.bit_size, 0u);
  DCHECK_LE(prefix.bit_size, 8u);

  uint8 next_octet = 0;
  if (!PeekNextOctet(&next_octet))
    return false;

  if ((next_octet >> (8 - prefix.bit_size)) == prefix.bits) {
    bit_offset_ = prefix.bit_size;
    return true;
  }

  return false;
}

bool HpackInputStream::PeekNextOctet(uint8* next_octet) {
  if ((bit_offset_ > 0) || buffer_.empty())
    return false;

  *next_octet = buffer_[0];
  return true;
}

bool HpackInputStream::DecodeNextOctet(uint8* next_octet) {
  if (!PeekNextOctet(next_octet))
    return false;

  buffer_.remove_prefix(1);
  return true;
}

bool HpackInputStream::DecodeNextUint32(uint32* I) {
  size_t N = 8 - bit_offset_;
  DCHECK_GT(N, 0u);
  DCHECK_LE(N, 8u);

  bit_offset_ = 0;

  *I = 0;

  uint8 next_marker = (1 << N) - 1;
  uint8 next_octet = 0;
  if (!DecodeNextOctet(&next_octet))
    return false;
  *I = next_octet & next_marker;

  bool has_more = (*I == next_marker);
  size_t shift = 0;
  while (has_more && (shift < 32)) {
    uint8 next_octet = 0;
    if (!DecodeNextOctet(&next_octet))
      return false;
    has_more = (next_octet & 0x80) != 0;
    next_octet &= 0x7f;
    uint32 addend = next_octet << shift;
    // Check for overflow.
    if ((addend >> shift) != next_octet) {
      return false;
    }
    *I += addend;
    shift += 7;
  }

  return !has_more;
}

// TODO(akalin): Figure out how to handle the storage for
// Huffman-encoded sequences.
bool HpackInputStream::DecodeNextStringLiteral(StringPiece* str) {
  if (MatchPrefixAndConsume(kStringLiteralIdentityEncoded)) {
    uint32 size = 0;
    if (!DecodeNextUint32(&size))
      return false;

    if (size > max_string_literal_size_)
      return false;

    if (size > buffer_.size())
      return false;

    *str = StringPiece(buffer_.data(), size);
    buffer_.remove_prefix(size);
    return true;
  }

  // TODO(akalin): Handle Huffman-encoded sequences.

  return false;
}

}  // namespace net
