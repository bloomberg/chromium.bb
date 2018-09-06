// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/bloom_filter.h"

#include <stdint.h>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

int CountBits(const ByteVector& vector) {
  int bit_count = 0;
  for (size_t i = 0; i < vector.size(); ++i) {
    uint8_t byte = vector[i];
    for (int j = 0; j < 8; ++j) {
      if (byte & (1 << j))
        bit_count++;
    }
  }
  return bit_count;
}

}  // namespace

TEST(BloomFilterTest, SingleHash) {
  ByteVector data(2, 0);
  BloomFilter filter(16 /* num_bits */, data, 1 /* num_hash_functions */);
  EXPECT_EQ(2u, filter.bytes().size());
  EXPECT_EQ(0, CountBits(filter.bytes()));

  EXPECT_FALSE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Alfa");
  EXPECT_EQ(1, CountBits(filter.bytes()));
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Bravo");
  filter.Add("Chuck");
  EXPECT_EQ(3, CountBits(filter.bytes()));
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_TRUE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));
}

TEST(BloomFilterTest, FalsePositivesWithSingleBitFilterCollisions) {
  ByteVector data(1, 0);
  BloomFilter filter(1 /* num_bits */, data, 1 /* num_hash_functions */);

  EXPECT_FALSE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Alfa");
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_TRUE(filter.Contains("Bravo"));
  EXPECT_TRUE(filter.Contains("Charlie"));
}

TEST(BloomFilterTest, MultiHash) {
  ByteVector data(10, 0);
  BloomFilter filter(75 /* num_bits */, data, 3 /* num_hash_functions */);
  EXPECT_EQ(10u, filter.bytes().size());
  EXPECT_EQ(0, CountBits(filter.bytes()));

  EXPECT_FALSE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Alfa");
  EXPECT_EQ(3, CountBits(filter.bytes()));
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Bravo");
  filter.Add("Chuck");
  EXPECT_EQ(9, CountBits(filter.bytes()));
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_TRUE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));
}

TEST(BloomFilterTest, EverythingMatches) {
  // Set all bits ON in byte vector.
  ByteVector data(1024, 0xff);
  BloomFilter filter(8191 /* num_bits */, data, 7 /* num_hash_functions */);

  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_TRUE(filter.Contains("Bravo"));
  EXPECT_TRUE(filter.Contains("Charlie"));
  EXPECT_TRUE(filter.Contains("Delta"));
  EXPECT_TRUE(filter.Contains("Echo"));
}

#if !defined(OS_IOS)
TEST(BloomFilterTest, ByteVectorTooSmall) {
  ByteVector data(1023, 0xff);
  EXPECT_DEATH({ BloomFilter filter(8191 /* num_bits */, data, 7); },
               "Check failed");
}
#endif

}  // namespace previews
