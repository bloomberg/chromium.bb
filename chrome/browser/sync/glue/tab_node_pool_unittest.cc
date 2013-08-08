// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/tab_node_pool.h"
#include "base/logging.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

class SyncTabNodePoolTest : public testing::Test {
 protected:
  SyncTabNodePoolTest() : pool_(NULL) { pool_.SetMachineTag("tag"); }

  int GetMaxUsedTabNodeId() const { return pool_.max_used_tab_node_id_; }

  void AddFreeTabNodes(size_t size, const int node_ids[]);

  TabNodePool pool_;
};

void SyncTabNodePoolTest::AddFreeTabNodes(size_t size, const int node_ids[]) {
  for (size_t i = 0; i < size; ++i) {
    pool_.free_nodes_pool_.insert(node_ids[i]);
  }
}

namespace {

TEST_F(SyncTabNodePoolTest, TabNodeIdIncreases) {
  // max_used_tab_node_ always increases.
  pool_.AddTabNode(10);
  EXPECT_EQ(10, GetMaxUsedTabNodeId());
  pool_.AddTabNode(5);
  EXPECT_EQ(10, GetMaxUsedTabNodeId());
  pool_.AddTabNode(1000);
  EXPECT_EQ(1000, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(1000, 1);
  pool_.ReassociateTabNode(5, 2);
  pool_.ReassociateTabNode(10, 3);
  // Freeing a tab node does not change max_used_tab_node_id_.
  pool_.FreeTabNode(1000);
  pool_.FreeTabNode(5);
  pool_.FreeTabNode(10);
  for (int i = 0; i < 3; ++i) {
    pool_.AssociateTabNode(pool_.GetFreeTabNode(), i + 1);
    EXPECT_EQ(1000, GetMaxUsedTabNodeId());
  }

  EXPECT_EQ(1000, GetMaxUsedTabNodeId());
  EXPECT_TRUE(pool_.Empty());
}

TEST_F(SyncTabNodePoolTest, OldTabNodesAddAndRemove) {
  // VerifyOldTabNodes are added.
  pool_.AddTabNode(1);
  pool_.AddTabNode(2);
  EXPECT_EQ(2u, pool_.Capacity());
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.IsUnassociatedTabNode(1));
  EXPECT_TRUE(pool_.IsUnassociatedTabNode(2));
  pool_.ReassociateTabNode(1, 2);
  EXPECT_TRUE(pool_.Empty());
  pool_.AssociateTabNode(2, 3);
  EXPECT_FALSE(pool_.IsUnassociatedTabNode(1));
  EXPECT_FALSE(pool_.IsUnassociatedTabNode(2));
  pool_.FreeTabNode(2);
  // 2 should be returned to free node pool_.
  EXPECT_EQ(2u, pool_.Capacity());
  // Should be able to free 1.
  pool_.FreeTabNode(1);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(1, pool_.GetFreeTabNode());
  pool_.AssociateTabNode(1, 1);
  EXPECT_EQ(2, pool_.GetFreeTabNode());
  pool_.AssociateTabNode(2, 1);
  EXPECT_TRUE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
}

TEST_F(SyncTabNodePoolTest, OldTabNodesReassociation) {
  // VerifyOldTabNodes are reassociated correctly.
  pool_.AddTabNode(4);
  pool_.AddTabNode(5);
  pool_.AddTabNode(6);
  EXPECT_EQ(3u, pool_.Capacity());
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.IsUnassociatedTabNode(4));
  pool_.ReassociateTabNode(4, 5);
  pool_.AssociateTabNode(5, 6);
  pool_.AssociateTabNode(6, 7);
  // Free 5 and 6.
  pool_.FreeTabNode(5);
  pool_.FreeTabNode(6);
  // 5 and 6 nodes should not be unassociated.
  EXPECT_FALSE(pool_.IsUnassociatedTabNode(5));
  EXPECT_FALSE(pool_.IsUnassociatedTabNode(6));
  // Free node pool should have 5 and 6.
  EXPECT_FALSE(pool_.Empty());
  EXPECT_EQ(3u, pool_.Capacity());

  // Free all nodes
  pool_.FreeTabNode(4);
  EXPECT_TRUE(pool_.Full());
  std::set<int> free_sync_ids;
  for (int i = 0; i < 3; ++i) {
    free_sync_ids.insert(pool_.GetFreeTabNode());
    // GetFreeTabNode will return the same value till the node is
    // reassociated.
    pool_.AssociateTabNode(pool_.GetFreeTabNode(), i + 1);
  }

  EXPECT_TRUE(pool_.Empty());
  EXPECT_EQ(3u, free_sync_ids.size());
  EXPECT_EQ(1u, free_sync_ids.count(4));
  EXPECT_EQ(1u, free_sync_ids.count(5));
  EXPECT_EQ(1u, free_sync_ids.count(6));
}

TEST_F(SyncTabNodePoolTest, Init) {
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
}

TEST_F(SyncTabNodePoolTest, AddGet) {
  int free_nodes[] = {5, 10};
  AddFreeTabNodes(2, free_nodes);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_EQ(5, pool_.GetFreeTabNode());
  pool_.AssociateTabNode(5, 1);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  // 5 is now used, should return 10.
  EXPECT_EQ(10, pool_.GetFreeTabNode());
}

TEST_F(SyncTabNodePoolTest, All) {
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(0U, pool_.Capacity());
  int free_nodes[] = {5, 10};
  AddFreeTabNodes(2, free_nodes);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  // GetFreeTabNode returns the lowest numbered free node.
  EXPECT_EQ(5, pool_.GetFreeTabNode());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  // Associate 5, next free node should be 10.
  pool_.AssociateTabNode(5, 1);
  EXPECT_EQ(10, pool_.GetFreeTabNode());
  pool_.AssociateTabNode(10, 2);
  EXPECT_TRUE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  // Release them in reverse order.
  pool_.FreeTabNode(10);
  pool_.FreeTabNode(5);
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(5, pool_.GetFreeTabNode());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  pool_.AssociateTabNode(5, 1);
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_EQ(10, pool_.GetFreeTabNode());
  pool_.AssociateTabNode(10, 2);
  EXPECT_TRUE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  // Release them again.
  pool_.FreeTabNode(10);
  pool_.FreeTabNode(5);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  pool_.Clear();
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(0U, pool_.Capacity());
}

}  // namespace

}  // namespace browser_sync
