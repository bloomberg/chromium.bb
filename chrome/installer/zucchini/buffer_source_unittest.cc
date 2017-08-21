// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/buffer_source.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

using vec = std::vector<uint8_t>;

constexpr size_t kLen = 10;
constexpr uint8_t bytes[kLen] = {0x10, 0x32, 0x54, 0x76, 0x98,
                                 0xBA, 0xDC, 0xFE, 0x10, 0x00};

class BufferSourceTest : public testing::Test {
 protected:
  BufferSource source_ = {std::begin(bytes), kLen};
};

TEST_F(BufferSourceTest, Skip) {
  EXPECT_EQ(kLen, source_.Remaining());
  source_.Skip(2);
  EXPECT_EQ(kLen - 2, source_.Remaining());
  source_.Skip(10);  // Skipping past end just moves cursor to end.
  EXPECT_EQ(size_t(0), source_.Remaining());
}

TEST_F(BufferSourceTest, CheckNextBytes) {
  EXPECT_TRUE(source_.CheckNextBytes({0x10, 0x32, 0x54, 0x76}));
  source_.Skip(4);
  EXPECT_TRUE(source_.CheckNextBytes({0x98, 0xBA, 0xDC, 0xFE}));

  // Cursor has not advanced, so check fails.
  EXPECT_FALSE(source_.CheckNextBytes({0x10, 0x00}));

  source_.Skip(4);
  EXPECT_EQ(size_t(2), source_.Remaining());

  // Goes beyond end by 2 bytes.
  EXPECT_FALSE(source_.CheckNextBytes({0x10, 0x00, 0x00, 0x00}));
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, ConsumeBytes) {
  EXPECT_FALSE(source_.ConsumeBytes({0x10, 0x00}));
  EXPECT_EQ(kLen, source_.Remaining());
  EXPECT_TRUE(source_.ConsumeBytes({0x10, 0x32, 0x54, 0x76}));
  EXPECT_EQ(size_t(6), source_.Remaining());
  EXPECT_TRUE(source_.ConsumeBytes({0x98, 0xBA, 0xDC, 0xFE}));
  EXPECT_EQ(size_t(2), source_.Remaining());

  // Goes beyond end by 2 bytes.
  EXPECT_FALSE(source_.ConsumeBytes({0x10, 0x00, 0x00, 0x00}));
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, CheckNextValue) {
  EXPECT_TRUE(source_.CheckNextValue(uint32_t(0x76543210)));
  EXPECT_FALSE(source_.CheckNextValue(uint32_t(0x0)));
  EXPECT_TRUE(source_.CheckNextValue(uint64_t(0xFEDCBA9876543210)));
  EXPECT_FALSE(source_.CheckNextValue(uint64_t(0x0)));

  source_.Skip(8);
  EXPECT_EQ(size_t(2), source_.Remaining());

  // Goes beyond end by 2 bytes.
  EXPECT_FALSE(source_.CheckNextValue(uint32_t(0x1000)));
}

// Supported by MSVC, g++, and clang++.
// Ensures no gaps in packing.
#pragma pack(push, 1)
struct ValueType {
  uint32_t a;
  uint16_t b;
};
#pragma pack(pop)

TEST_F(BufferSourceTest, GetValueIntegral) {
  uint32_t value = 0;
  EXPECT_TRUE(source_.GetValue(&value));
  EXPECT_EQ(uint32_t(0x76543210), value);
  EXPECT_EQ(size_t(6), source_.Remaining());

  EXPECT_TRUE(source_.GetValue(&value));
  EXPECT_EQ(uint32_t(0xFEDCBA98), value);
  EXPECT_EQ(size_t(2), source_.Remaining());

  EXPECT_FALSE(source_.GetValue(&value));
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, GetValueAggregate) {
  ValueType value = {};
  EXPECT_TRUE(source_.GetValue(&value));
  EXPECT_EQ(uint32_t(0x76543210), value.a);
  EXPECT_EQ(uint32_t(0xBA98), value.b);
  EXPECT_EQ(size_t(4), source_.Remaining());
}

TEST_F(BufferSourceTest, GetRegion) {
  ConstBufferView region;
  EXPECT_TRUE(source_.GetRegion(0, &region));
  EXPECT_EQ(kLen, source_.Remaining());
  EXPECT_TRUE(region.empty());

  EXPECT_TRUE(source_.GetRegion(2, &region));
  EXPECT_EQ(size_t(2), region.size());
  EXPECT_EQ(vec({0x10, 0x32}), vec(region.begin(), region.end()));
  EXPECT_EQ(size_t(8), source_.Remaining());

  EXPECT_FALSE(source_.GetRegion(kLen, &region));
  EXPECT_EQ(size_t(8), source_.Remaining());
  // |region| is left untouched.
  EXPECT_EQ(vec({0x10, 0x32}), vec(region.begin(), region.end()));
  EXPECT_EQ(size_t(2), region.size());
}

TEST_F(BufferSourceTest, GetPointerIntegral) {
  const uint32_t* ptr = source_.GetPointer<uint32_t>();
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x76543210), *ptr);
  EXPECT_EQ(size_t(6), source_.Remaining());

  ptr = source_.GetPointer<uint32_t>();
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0xFEDCBA98), *ptr);
  EXPECT_EQ(size_t(2), source_.Remaining());

  EXPECT_EQ(nullptr, source_.GetPointer<uint32_t>());
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, GetPointerAggregate) {
  const ValueType* ptr = source_.GetPointer<ValueType>();
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x76543210), ptr->a);
  EXPECT_EQ(uint32_t(0xBA98), ptr->b);
  EXPECT_EQ(size_t(4), source_.Remaining());
}

TEST_F(BufferSourceTest, GetArrayIntegral) {
  EXPECT_EQ(nullptr, source_.GetArray<uint32_t>(3));

  const uint32_t* ptr = source_.GetArray<uint32_t>(2);
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x76543210), ptr[0]);
  EXPECT_EQ(uint32_t(0xFEDCBA98), ptr[1]);
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, GetArrayAggregate) {
  const ValueType* ptr = source_.GetArray<ValueType>(2);
  EXPECT_EQ(nullptr, ptr);

  ptr = source_.GetArray<ValueType>(1);

  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x76543210), ptr[0].a);
  EXPECT_EQ(uint32_t(0xBA98), ptr[0].b);
  EXPECT_EQ(size_t(4), source_.Remaining());
}

}  // namespace zucchini
