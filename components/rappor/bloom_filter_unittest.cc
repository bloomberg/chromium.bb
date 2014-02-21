// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/bloom_filter.h"

#include "components/rappor/byte_vector_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace rappor {

TEST(BloomFilterTest, TinyFilter) {
  BloomFilter filter(1u, 4u, 0u);

  // Size is 1 and it's initially empty
  EXPECT_EQ(1u, filter.bytes().size());
  EXPECT_EQ(0x00, filter.bytes()[0]);

  // "Test" has a self-collision, and only sets 3 bits.
  filter.AddString("Test");
  EXPECT_EQ(0x2a, filter.bytes()[0]);

  // Adding the same value shouldn't change anything.
  filter.AddString("Test");
  EXPECT_EQ(0x2a, filter.bytes()[0]);

  BloomFilter filter2(1u, 4u, 0u);
  EXPECT_EQ(0x00, filter2.bytes()[0]);
  filter2.AddString("Bar");
  EXPECT_EQ(0xa8, filter2.bytes()[0]);

  // Adding a colliding string should just set new bits.
  filter.AddString("Bar");
  EXPECT_EQ(0xaa, filter.bytes()[0]);
}

TEST(BloomFilterTest, HugeFilter) {
  // Create a 500 bit filter, and use a large seed offset to see if anything
  // breaks.
  BloomFilter filter(500u, 1u, 0xabdef123);

  // Size is 500 and it's initially empty
  EXPECT_EQ(500u, filter.bytes().size());
  EXPECT_EQ(0, CountBits(filter.bytes()));

  filter.AddString("Bar");
  EXPECT_EQ(1, CountBits(filter.bytes()));

  // Adding the same value shouldn't change anything.
  filter.AddString("Bar");
  EXPECT_EQ(1, CountBits(filter.bytes()));
}

}  // namespace rappor
