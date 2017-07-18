// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/patch_utils.h"

#include <cstdint>
#include <iterator>
#include <vector>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

template <class T>
void TestEncodeDecodeVarUInt(const std::vector<T>& data) {
  std::vector<uint8_t> buffer;

  std::vector<T> values;
  for (T basis : data) {
    // For variety, test the neighborhood values for each case in |data|. Some
    // test cases may result in overflow when computing |value|, but we don't
    // care about that.
    for (int delta = -4; delta <= 4; ++delta) {
      T value = delta + basis;
      EncodeVarUInt<T>(value, std::back_inserter(buffer));
      values.push_back(value);

      value = delta - basis;
      EncodeVarUInt<T>(value, std::back_inserter(buffer));
      values.push_back(value);
    }
  }

  auto it = buffer.begin();
  for (T expected : values) {
    T value = T(-1);
    auto res = DecodeVarUInt(it, buffer.end(), &value);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(expected, value);
    it = res.value();
  }
  EXPECT_EQ(it, buffer.end());

  T dummy = T(-1);
  auto res = DecodeVarUInt(it, buffer.end(), &dummy);
  EXPECT_EQ(base::nullopt, res);
  EXPECT_EQ(T(-1), dummy);
}

template <class T>
void TestEncodeDecodeVarInt(const std::vector<T>& data) {
  std::vector<uint8_t> buffer;

  std::vector<T> values;
  for (T basis : data) {
    // For variety, test the neighborhood values for each case in |data|. Some
    // test cases may result in overflow when computing |value|, but we don't
    // care about that.
    for (int delta = -4; delta <= 4; ++delta) {
      T value = delta + basis;
      EncodeVarInt(value, std::back_inserter(buffer));
      values.push_back(value);

      value = delta - basis;
      EncodeVarInt(value, std::back_inserter(buffer));
      values.push_back(value);
    }
  }

  auto it = buffer.begin();
  for (T expected : values) {
    T value = T(-1);
    auto res = DecodeVarInt(it, buffer.end(), &value);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(expected, value);
    it = res.value();
  }
  T dummy = T(-1);
  auto res = DecodeVarInt(it, buffer.end(), &dummy);
  EXPECT_EQ(base::nullopt, res);
  EXPECT_EQ(T(-1), dummy);
}

TEST(PatchUtilsTest, EncodeDecodeVarUInt32) {
  TestEncodeDecodeVarUInt<uint32_t>({0, 64, 128, 8192, 16384, 1 << 20, 1 << 21,
                                     1 << 22, 1 << 27, 1 << 28, 0x7FFFFFFF,
                                     UINT32_MAX});
}

TEST(PatchUtilsTest, EncodeDecodeVarInt32) {
  TestEncodeDecodeVarInt<int32_t>({0, 64, 128, 8192, 16384, 1 << 20, 1 << 21,
                                   1 << 22, 1 << 27, 1 << 28, -1, INT32_MIN,
                                   INT32_MAX});
}

TEST(PatchUtilsTest, EncodeDecodeVarUInt64) {
  TestEncodeDecodeVarUInt<uint64_t>({0, 64, 128, 8192, 16384, 1 << 20, 1 << 21,
                                     1 << 22, 1ULL << 55, 1ULL << 56,
                                     0x7FFFFFFFFFFFFFFF, UINT64_MAX});
}

TEST(PatchUtilsTest, EncodeDecodeVarInt64) {
  TestEncodeDecodeVarInt<int64_t>({0, 64, 128, 8192, 16384, 1 << 20, 1 << 21,
                                   1 << 22, 1LL << 55, 1LL << 56, -1, INT64_MIN,
                                   INT64_MAX});
}

TEST(PatchUtilsTest, DecodeVarUInt32Malformed) {
  // Dummy variable to ensure that on failure, the output variable is not
  // written to.
  uint32_t dummy = uint32_t(-1);

  auto TestDecodeVarInt = [&dummy](const std::vector<uint8_t>& buffer) {
    dummy = uint32_t(-1);
    return DecodeVarUInt(buffer.begin(), buffer.end(), &dummy);
  };

  // Exhausted.
  EXPECT_EQ(base::nullopt, TestDecodeVarInt(std::vector<uint8_t>{}));
  EXPECT_EQ(uint32_t(-1), dummy);
  EXPECT_EQ(base::nullopt, TestDecodeVarInt(std::vector<uint8_t>(4, 128)));
  EXPECT_EQ(uint32_t(-1), dummy);

  // Overflow.
  EXPECT_EQ(base::nullopt, TestDecodeVarInt(std::vector<uint8_t>(6, 128)));
  EXPECT_EQ(uint32_t(-1), dummy);
  EXPECT_EQ(base::nullopt, TestDecodeVarInt({128, 128, 128, 128, 128, 42}));
  EXPECT_EQ(uint32_t(-1), dummy);

  // Following are pathological cases that are not handled for simplicity,
  // hence decoding is expected to be successful.
  EXPECT_NE(base::nullopt, TestDecodeVarInt({128, 128, 128, 128, 16}));
  EXPECT_EQ(uint32_t(0), dummy);
  EXPECT_NE(base::nullopt, TestDecodeVarInt({128, 128, 128, 128, 32}));
  EXPECT_EQ(uint32_t(0), dummy);
  EXPECT_NE(base::nullopt, TestDecodeVarInt({128, 128, 128, 128, 64}));
  EXPECT_EQ(uint32_t(0), dummy);
}

TEST(PatchUtilsTest, DecodeVarUInt64Malformed) {
  uint64_t dummy = uint64_t(-1);
  auto TestDecodeVarInt = [&dummy](const std::vector<uint8_t>& buffer) {
    return DecodeVarUInt(buffer.begin(), buffer.end(), &dummy);
  };

  // Exhausted.
  EXPECT_EQ(base::nullopt, TestDecodeVarInt(std::vector<uint8_t>{}));
  EXPECT_EQ(uint64_t(-1), dummy);
  EXPECT_EQ(base::nullopt, TestDecodeVarInt(std::vector<uint8_t>(9, 128)));
  EXPECT_EQ(uint64_t(-1), dummy);

  // Overflow.
  EXPECT_EQ(base::nullopt, TestDecodeVarInt(std::vector<uint8_t>(10, 128)));
  EXPECT_EQ(uint64_t(-1), dummy);
  EXPECT_EQ(base::nullopt, TestDecodeVarInt({128, 128, 128, 128, 128, 128, 128,
                                             128, 128, 128, 42}));
  EXPECT_EQ(uint64_t(-1), dummy);
}

}  // namespace zucchini
