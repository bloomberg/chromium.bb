// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/tab_node_pool.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

typedef testing::Test SyncTabNodePoolTest;

TEST_F(SyncTabNodePoolTest, Init) {
  TabNodePool pool(NULL);
  pool.set_machine_tag("tag");
  ASSERT_TRUE(pool.empty());
  ASSERT_TRUE(pool.full());
}

TEST_F(SyncTabNodePoolTest, AddGet) {
  TabNodePool pool(NULL);
  pool.set_machine_tag("tag");

  pool.AddTabNode(5);
  pool.AddTabNode(10);
  ASSERT_FALSE(pool.empty());
  ASSERT_TRUE(pool.full());

  ASSERT_EQ(2U, pool.capacity());
  ASSERT_EQ(10, pool.GetFreeTabNode());  // Returns last free tab.
  ASSERT_FALSE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_EQ(5, pool.GetFreeTabNode());  // Returns last free tab.
}

TEST_F(SyncTabNodePoolTest, All) {
  TabNodePool pool(NULL);
  pool.set_machine_tag("tag");
  ASSERT_TRUE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(0U, pool.capacity());
  pool.AddTabNode(5);
  pool.AddTabNode(10);
  ASSERT_FALSE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_EQ(10, pool.GetFreeTabNode());  // Returns last free tab.
  ASSERT_FALSE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_EQ(5, pool.GetFreeTabNode());  // Returns last free tab.
  ASSERT_TRUE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  // Release them in reverse order.
  pool.FreeTabNode(10);
  pool.FreeTabNode(5);
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(5, pool.GetFreeTabNode());  // Returns last free tab.
  ASSERT_FALSE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  ASSERT_EQ(10, pool.GetFreeTabNode());  // Returns last free tab.
  ASSERT_TRUE(pool.empty());
  ASSERT_FALSE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  // Release them again.
  pool.FreeTabNode(10);
  pool.FreeTabNode(5);
  ASSERT_FALSE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(2U, pool.capacity());
  pool.clear();
  ASSERT_TRUE(pool.empty());
  ASSERT_TRUE(pool.full());
  ASSERT_EQ(0U, pool.capacity());
}

}  // namespace

}  // namespace browser_sync
