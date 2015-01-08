// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/packed_ct_ev_whitelist/bit_stream_reader.h"

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace packed_ct_ev_whitelist {
namespace internal {

BitStreamReader::BitStreamReader(const base::StringPiece& source)
    : source_(source), current_byte_(0), current_bit_(7) {
  DCHECK_LT(source_.length(), UINT32_MAX);
}

bool BitStreamReader::ReadUnaryEncoding(uint64_t* out) {
  if (BitsLeft() == 0)
    return false;

  *out = 0;
  while ((BitsLeft() > 0) && ReadBit())
    ++(*out);

  return true;
}

bool BitStreamReader::ReadBits(uint8_t num_bits, uint64_t* out) {
  DCHECK_LE(num_bits, 64);

  if (BitsLeft() < num_bits)
    return false;

  *out = 0;
  for (uint8_t i = 0; i < num_bits; ++i)
    (*out) |= (static_cast<uint64_t>(ReadBit()) << (num_bits - (i + 1)));

  return true;
}

uint64_t BitStreamReader::BitsLeft() const {
  if (current_byte_ == source_.length())
    return 0;
  DCHECK_GT(source_.length(), current_byte_);
  return (source_.length() - (current_byte_ + 1)) * 8 + current_bit_ + 1;
}

uint8_t BitStreamReader::ReadBit() {
  DCHECK_GT(BitsLeft(), 0u);
  DCHECK(current_bit_ < 8 && current_bit_ >= 0);
  uint8_t res =
      (source_.data()[current_byte_] & (1 << current_bit_)) >> current_bit_;
  current_bit_--;
  if (current_bit_ < 0) {
    current_byte_++;
    current_bit_ = 7;
  }

  return res;
}

}  // namespace internal
}  // namespace packed_ct_ev_whitelist
