// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/h264_bitstream_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
const uint64 kTestPattern = 0xfedcba0987654321;
}

class H264BitstreamBufferAppendBitsTest
    : public ::testing::TestWithParam<uint64> {};

// TODO(posciak): More tests!

TEST_P(H264BitstreamBufferAppendBitsTest, AppendAndVerifyBits) {
  H264BitstreamBuffer b;
  uint64 num_bits = GetParam();
  // TODO(posciak): Tests for >64 bits.
  ASSERT_LE(num_bits, 64u);
  uint64 num_bytes = (num_bits + 7) / 8;

  b.AppendBits(num_bits, kTestPattern);
  b.FlushReg();

  EXPECT_EQ(b.BytesInBuffer(), num_bytes);

  uint8* ptr = b.data();
  uint64 got = 0;
  uint64 expected = kTestPattern;

  if (num_bits < 64)
    expected &= ((1ull << num_bits) - 1);

  while (num_bits > 8) {
    got |= (*ptr & 0xff);
    num_bits -= 8;
    got <<= (num_bits > 8 ? 8 : num_bits);
    ptr++;
  }
  if (num_bits > 0) {
    uint64 temp = (*ptr & 0xff);
    temp >>= (8 - num_bits);
    got |= temp;
  }
  EXPECT_EQ(got, expected) << std::hex << "0x" << got << " vs 0x" << expected;
}

INSTANTIATE_TEST_CASE_P(AppendNumBits,
                        H264BitstreamBufferAppendBitsTest,
                        ::testing::Range(static_cast<uint64>(1),
                                         static_cast<uint64>(65)));
}  // namespace media
