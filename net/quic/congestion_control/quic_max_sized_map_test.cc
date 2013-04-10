// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/quic/congestion_control/quic_max_sized_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class QuicMaxSizedMapTest : public ::testing::Test {
};

TEST_F(QuicMaxSizedMapTest, Basic) {
  QuicMaxSizedMap<int, int> test_map(100);
  EXPECT_EQ(100u, test_map.MaxSize());
  EXPECT_EQ(0u, test_map.Size());
  test_map.Insert(1, 2);
  test_map.Insert(1, 3);
  EXPECT_EQ(100u, test_map.MaxSize());
  EXPECT_EQ(2u, test_map.Size());
  test_map.RemoveAll();
  EXPECT_EQ(100u, test_map.MaxSize());
  EXPECT_EQ(0u, test_map.Size());
}

TEST_F(QuicMaxSizedMapTest, Find) {
  QuicMaxSizedMap<int, int> test_map(100);
  test_map.Insert(1, 2);
  test_map.Insert(1, 3);
  test_map.Insert(2, 4);
  test_map.Insert(3, 5);
  QuicMaxSizedMap<int, int>::ConstIterator it = test_map.Find(2);
  EXPECT_TRUE(it != test_map.End());
  EXPECT_EQ(4, it->second);
  it = test_map.Find(1);
  EXPECT_TRUE(it != test_map.End());
  EXPECT_EQ(2, it->second);
  ++it;
  EXPECT_TRUE(it != test_map.End());
  EXPECT_EQ(3, it->second);
}

TEST_F(QuicMaxSizedMapTest, Sort) {
  QuicMaxSizedMap<int, int> test_map(100);
  test_map.Insert(9, 9);
  test_map.Insert(8, 8);
  test_map.Insert(7, 7);
  test_map.Insert(6, 6);
  test_map.Insert(2, 2);
  test_map.Insert(4, 4);
  test_map.Insert(5, 5);
  test_map.Insert(3, 3);
  test_map.Insert(0, 0);
  test_map.Insert(1, 1);
  QuicMaxSizedMap<int, int>::ConstIterator it = test_map.Begin();
  for (int i = 0; i < 10; ++i, ++it) {
    EXPECT_TRUE(it != test_map.End());
    EXPECT_EQ(i, it->first);
    EXPECT_EQ(i, it->second);
  }
}

}  // namespace test
}  // namespace net
