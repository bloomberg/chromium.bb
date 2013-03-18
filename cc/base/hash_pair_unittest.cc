// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/hash_pair.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HashPairTest : public testing::Test {
};

#define INSERT_PAIR_TEST(Type, value1, value2) \
  { \
    Type pair(value1, value2); \
    base::hash_map<Type, int> map; \
    map[pair] = 1; \
  }

// Verify that a hash_map can be constructed for pairs of integers of various
// sizes.
TEST_F(HashPairTest, IntegerPairs) {
  typedef std::pair<short, short> ShortShortPair;
  typedef std::pair<short, int> ShortIntPair;
  typedef std::pair<short, int32> ShortInt32Pair;
  typedef std::pair<short, int64> ShortInt64Pair;

  INSERT_PAIR_TEST(ShortShortPair, 4, 6);
  INSERT_PAIR_TEST(ShortIntPair, 7, (1 << 30) + 4342);
  INSERT_PAIR_TEST(ShortInt32Pair, 9, (1 << 29) + 378128932);
  INSERT_PAIR_TEST(ShortInt64Pair, 10,
                   (GG_INT64_C(1) << 60) + GG_INT64_C(78931732321));

  typedef std::pair<int, short> IntShortPair;
  typedef std::pair<int, int> IntIntPair;
  typedef std::pair<int, int32> IntInt32Pair;
  typedef std::pair<int, int64> IntInt64Pair;

  INSERT_PAIR_TEST(IntShortPair, 4, 6);
  INSERT_PAIR_TEST(IntIntPair, 7, (1 << 30) + 4342);
  INSERT_PAIR_TEST(IntInt32Pair, 9, (1 << 29) + 378128932);
  INSERT_PAIR_TEST(IntInt64Pair, 10,
                   (GG_INT64_C(1) << 60) + GG_INT64_C(78931732321));

  typedef std::pair<int32, short> Int32ShortPair;
  typedef std::pair<int32, int> Int32IntPair;
  typedef std::pair<int32, int32> Int32Int32Pair;
  typedef std::pair<int32, int64> Int32Int64Pair;

  INSERT_PAIR_TEST(Int32ShortPair, 4, 6);
  INSERT_PAIR_TEST(Int32IntPair, 7, (1 << 30) + 4342);
  INSERT_PAIR_TEST(Int32Int32Pair, 9, (1 << 29) + 378128932);
  INSERT_PAIR_TEST(Int32Int64Pair, 10,
                   (GG_INT64_C(1) << 60) + GG_INT64_C(78931732321));

  typedef std::pair<int64, short> Int64ShortPair;
  typedef std::pair<int64, int> Int64IntPair;
  typedef std::pair<int64, int32> Int64Int32Pair;
  typedef std::pair<int64, int64> Int64Int64Pair;

  INSERT_PAIR_TEST(Int64ShortPair, 4, 6);
  INSERT_PAIR_TEST(Int64IntPair, 7, (1 << 30) + 4342);
  INSERT_PAIR_TEST(Int64Int32Pair, 9, (1 << 29) + 378128932);
  INSERT_PAIR_TEST(Int64Int64Pair, 10,
                   (GG_INT64_C(1) << 60) + GG_INT64_C(78931732321));
}

}  // namespace
