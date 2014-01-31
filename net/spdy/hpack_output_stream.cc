// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_output_stream.h"

#include "base/logging.h"

using base::StringPiece;

namespace net {

using std::string;

HpackOutputStream::HpackOutputStream(uint32 max_string_literal_size)
    : max_string_literal_size_(max_string_literal_size),
      bit_offset_(0) {}

HpackOutputStream::~HpackOutputStream() {}

void HpackOutputStream::AppendIndexedHeader(uint32 index_or_zero) {
  AppendPrefix(kIndexedOpcode);
  AppendUint32(index_or_zero);
}

bool HpackOutputStream::AppendLiteralHeaderNoIndexingWithName(
    StringPiece name, StringPiece value) {
  AppendPrefix(kLiteralNoIndexOpcode);
  AppendBits(0x0, 8 - kLiteralNoIndexOpcode.bit_size);
  if (!AppendStringLiteral(name))
    return false;
  if (!AppendStringLiteral(value))
    return false;
  return true;
}

void HpackOutputStream::TakeString(string* output) {
  // This must hold, since all public functions cause the buffer to
  // end on a byte boundary.
  DCHECK_EQ(bit_offset_, 0u);
  buffer_.swap(*output);
  buffer_.clear();
  bit_offset_ = 0;
}

void HpackOutputStream::AppendBits(uint8 bits, size_t bit_size) {
  DCHECK_GT(bit_size, 0u);
  DCHECK_LE(bit_size, 8u);
  DCHECK_EQ(bits >> bit_size, 0);
  size_t new_bit_offset = bit_offset_ + bit_size;
  if (bit_offset_ == 0) {
    // Buffer ends on a byte boundary.
    DCHECK_LE(bit_size, 8u);
    buffer_.append(1, bits << (8 - bit_size));
  } else if (new_bit_offset <= 8) {
    // Buffer does not end on a byte boundary but the given bits fit
    // in the remainder of the last byte.
    *buffer_.rbegin() |= bits << (8 - new_bit_offset);
  } else {
    // Buffer does not end on a byte boundary and the given bits do
    // not fit in the remainder of the last byte.
    *buffer_.rbegin() |= bits >> (new_bit_offset - 8);
    buffer_.append(1, bits << (16 - new_bit_offset));
  }
  bit_offset_ = new_bit_offset % 8;
}

void HpackOutputStream::AppendPrefix(HpackPrefix prefix) {
  AppendBits(prefix.bits, prefix.bit_size);
}

void HpackOutputStream::AppendUint32(uint32 I) {
  // The algorithm below is adapted from the pseudocode in 4.1.1.
  size_t N = 8 - bit_offset_;
  uint8 max_first_byte = static_cast<uint8>((1 << N) - 1);
  if (I < max_first_byte) {
    AppendBits(static_cast<uint8>(I), N);
  } else {
    AppendBits(max_first_byte, N);
    I -= max_first_byte;
    while ((I & ~0x7f) != 0) {
      buffer_.append(1, (I & 0x7f) | 0x80);
      I >>= 7;
    }
    AppendBits(static_cast<uint8>(I), 8);
  }
}

bool HpackOutputStream::AppendStringLiteral(base::StringPiece str) {
  DCHECK_EQ(bit_offset_, 0u);
  // TODO(akalin): Implement Huffman encoding.
  AppendPrefix(kStringLiteralIdentityEncoded);
  if (str.size() > max_string_literal_size_)
    return false;
  AppendUint32(static_cast<uint32>(str.size()));
  buffer_.append(str.data(), str.size());
  return true;
}

}  // namespace net
