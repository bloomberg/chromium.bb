// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/test/chromedriver/synchronized_map.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SynchronizedMapTest, Set) {
  SynchronizedMap<int, int> map;
  map.Set(1, 2);
  ASSERT_TRUE(map.Has(1));
  int val = 0;
  ASSERT_TRUE(map.Get(1, &val));
  ASSERT_EQ(2, val);

  map.Set(1, 3);
  ASSERT_TRUE(map.Has(1));
  ASSERT_TRUE(map.Get(1, &val));
  ASSERT_EQ(3, val);

  map.Set(3, 1);
  ASSERT_TRUE(map.Has(1));
  ASSERT_TRUE(map.Get(1, &val));
  ASSERT_EQ(3, val);
  ASSERT_TRUE(map.Has(3));
  ASSERT_TRUE(map.Get(3, &val));
  ASSERT_EQ(1, val);
}

TEST(SynchronizedMapTest, Get) {
  SynchronizedMap<int, int> map;
  int val = 0;
  ASSERT_FALSE(map.Get(1, &val));
  map.Set(1, 2);
  ASSERT_TRUE(map.Get(1, &val));
  ASSERT_EQ(2, val);

  ASSERT_TRUE(map.Remove(1));
  val = 100;
  ASSERT_FALSE(map.Get(1, &val));
  ASSERT_EQ(100, val);
}

TEST(SynchronizedMapTest, Has) {
  SynchronizedMap<int, int> map;
  ASSERT_FALSE(map.Has(1));
  map.Set(1, 2);
  ASSERT_TRUE(map.Has(1));
  ASSERT_FALSE(map.Has(2));

  ASSERT_TRUE(map.Remove(1));
  ASSERT_FALSE(map.Has(1));
}

TEST(SynchronizedMapTest, GetKeys) {
  SynchronizedMap<int, int> map;

  std::vector<int> keys;
  map.GetKeys(&keys);
  ASSERT_EQ(0u, keys.size());

  keys.push_back(100);
  map.GetKeys(&keys);
  ASSERT_EQ(0u, keys.size());

  map.Set(1, 2);
  map.GetKeys(&keys);
  ASSERT_EQ(1u, keys.size());
  ASSERT_EQ(1, keys[0]);

  map.Set(2, 4);
  map.GetKeys(&keys);
  ASSERT_EQ(2u, keys.size());
  ASSERT_EQ(1, keys[0]);
  ASSERT_EQ(2, keys[1]);
}
