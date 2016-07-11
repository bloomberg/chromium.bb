// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/proto_utils.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace v2 {
namespace proto {
namespace {

bool CheckWriteVarIntU32(const char* expected, size_t length, uint32_t value) {
  uint8_t buf[32];
  uint8_t* res = WriteVarIntU32(value, buf);
  if (static_cast<size_t>(res - buf) != length)
    return false;
  if (memcmp(expected, buf, length))
    return false;
  return true;
}

bool CheckWriteVarIntU64(const char* expected, size_t length, uint64_t value) {
  uint8_t buf[32];
  uint8_t* res = WriteVarIntU64(value, buf);
  if (static_cast<size_t>(res - buf) != length)
    return false;
  if (memcmp(expected, buf, length))
    return false;
  return true;
}

#define EXPECT_VARINT32_EQ(expected, expected_length, value) \
  EXPECT_PRED3(CheckWriteVarIntU32, expected, expected_length, value)

#define EXPECT_VARINT64_EQ(expected, expected_length, value) \
  EXPECT_PRED3(CheckWriteVarIntU64, expected, expected_length, value)

TEST(ProtoUtilsTest, Serialization) {
  EXPECT_EQ(0x08u, MakeTagVarInt(1));
  EXPECT_EQ(0x09u, MakeTagFixed64(1));
  EXPECT_EQ(0x0Au, MakeTagLengthDelimited(1));
  EXPECT_EQ(0x0Du, MakeTagFixed32(1));

  EXPECT_EQ(0x03F8u, MakeTagVarInt(0x7F));
  EXPECT_EQ(0x03F9u, MakeTagFixed64(0x7F));
  EXPECT_EQ(0x03FAu, MakeTagLengthDelimited(0x7F));
  EXPECT_EQ(0x03FDu, MakeTagFixed32(0x7F));

  EXPECT_EQ(0x0400u, MakeTagVarInt(0x80));
  EXPECT_EQ(0x0401u, MakeTagFixed64(0x80));
  EXPECT_EQ(0x0402u, MakeTagLengthDelimited(0x80));
  EXPECT_EQ(0x0405u, MakeTagFixed32(0x80));

  EXPECT_EQ(0x01FFF8u, MakeTagVarInt(0x3fff));
  EXPECT_EQ(0x01FFF9u, MakeTagFixed64(0x3fff));
  EXPECT_EQ(0x01FFFAu, MakeTagLengthDelimited(0x3fff));
  EXPECT_EQ(0x01FFFDu, MakeTagFixed32(0x3fff));

  EXPECT_EQ(0x020000u, MakeTagVarInt(0x4000));
  EXPECT_EQ(0x020001u, MakeTagFixed64(0x4000));
  EXPECT_EQ(0x020002u, MakeTagLengthDelimited(0x4000));
  EXPECT_EQ(0x020005u, MakeTagFixed32(0x4000));

  EXPECT_VARINT32_EQ("\x00", 1, 0);
  EXPECT_VARINT32_EQ("\x00", 1, 0);
  EXPECT_VARINT32_EQ("\x01", 1, 0x1);
  EXPECT_VARINT32_EQ("\x7f", 1, 0x7F);
  EXPECT_VARINT32_EQ("\xFF\x01", 2, 0xFF);
  EXPECT_VARINT32_EQ("\xFF\x7F", 2, 0x3FFF);
  EXPECT_VARINT32_EQ("\x80\x80\x01", 3, 0x4000);
  EXPECT_VARINT32_EQ("\xFF\xFF\x7F", 3, 0x1FFFFF);
  EXPECT_VARINT32_EQ("\x80\x80\x80\x01", 4, 0x200000);
  EXPECT_VARINT32_EQ("\xFF\xFF\xFF\x7F", 4, 0xFFFFFFF);
  EXPECT_VARINT32_EQ("\x80\x80\x80\x80\x01", 5, 0x10000000);
  EXPECT_VARINT32_EQ("\xFF\xFF\xFF\xFF\x0F", 5, 0xFFFFFFFF);

  EXPECT_VARINT64_EQ("\x00", 1, 0);
  EXPECT_VARINT64_EQ("\x01", 1, 0x1);
  EXPECT_VARINT64_EQ("\x7f", 1, 0x7F);
  EXPECT_VARINT64_EQ("\xFF\x01", 2, 0xFF);
  EXPECT_VARINT64_EQ("\xFF\x7F", 2, 0x3FFF);
  EXPECT_VARINT64_EQ("\x80\x80\x01", 3, 0x4000);
  EXPECT_VARINT64_EQ("\xFF\xFF\x7F", 3, 0x1FFFFF);
  EXPECT_VARINT64_EQ("\x80\x80\x80\x01", 4, 0x200000);
  EXPECT_VARINT64_EQ("\xFF\xFF\xFF\x7F", 4, 0xFFFFFFF);
  EXPECT_VARINT64_EQ("\x80\x80\x80\x80\x01", 5, 0x10000000);
  EXPECT_VARINT64_EQ("\xFF\xFF\xFF\xFF\x0F", 5, 0xFFFFFFFF);
  EXPECT_VARINT64_EQ("\x80\x80\x80\x80\x10", 5, 0x100000000);
  EXPECT_VARINT64_EQ("\xFF\xFF\xFF\xFF\x7F", 5, 0x7FFFFFFFF);
  EXPECT_VARINT64_EQ("\x80\x80\x80\x80\x80\x01", 6, 0x800000000);
  EXPECT_VARINT64_EQ("\xFF\xFF\xFF\xFF\xFF\x7F", 6, 0x3FFFFFFFFFF);
  EXPECT_VARINT64_EQ("\x80\x80\x80\x80\x80\x80\x01", 7, 0x40000000000);
  EXPECT_VARINT64_EQ("\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 7, 0x1FFFFFFFFFFFF);
  EXPECT_VARINT64_EQ("\x80\x80\x80\x80\x80\x80\x80\x01", 8, 0x2000000000000);
  EXPECT_VARINT64_EQ("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 8, 0xFFFFFFFFFFFFFF);
  EXPECT_VARINT64_EQ("\x80\x80\x80\x80\x80\x80\x80\x80\x01", 9,
                     0x100000000000000);
  EXPECT_VARINT64_EQ("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 9,
                     0x7FFFFFFFFFFFFFFF);
  EXPECT_VARINT64_EQ("\x80\x80\x80\x80\x80\x80\x80\x80\x80\x01", 10,
                     0x8000000000000000);
  EXPECT_VARINT64_EQ("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01", 10,
                     0xFFFFFFFFFFFFFFFF);

  uint8_t buf[4];

  WriteRedundantVarIntU32<sizeof(buf)>(0, buf);
  EXPECT_EQ(0, memcmp("\x80\x80\x80\x00", buf, sizeof(buf)));

  WriteRedundantVarIntU32<sizeof(buf)>(1, buf);
  EXPECT_EQ(0, memcmp("\x81\x80\x80\x00", buf, sizeof(buf)));

  WriteRedundantVarIntU32<sizeof(buf)>(0x80, buf);
  EXPECT_EQ(0, memcmp("\x80\x81\x80\x00", buf, sizeof(buf)));

  WriteRedundantVarIntU32<sizeof(buf)>(0x332211, buf);
  EXPECT_EQ(0, memcmp("\x91\xC4\xCC\x01", buf, sizeof(buf)));

  WriteRedundantVarIntU32<sizeof(buf)>(0x0FFFFFFF, buf);
  EXPECT_EQ(0, memcmp("\xFF\xFF\xFF\x7F", buf, sizeof(buf)));
}

}  // namespace
}  // namespace proto
}  // namespace v2
}  // namespace tracing
