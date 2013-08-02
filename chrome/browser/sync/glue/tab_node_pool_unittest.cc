// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/tab_node_pool.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

class SyncTabNodePoolTest : public testing::Test {
 protected:
  SyncTabNodePoolTest() : pool_(NULL) { pool_.SetMachineTag("tag"); }

  int GetMaxUsedTabNodeId() const { return pool_.max_used_tab_node_id_; }

  TabNodePool pool_;
};

namespace {

TEST_F(SyncTabNodePoolTest, TabNodeIdIncreases) {
  // max_used_tab_node_ always increases.
  SessionID session_id;
  session_id.set_id(1);
  pool_.AddTabNode(10, session_id);
  EXPECT_EQ(10, GetMaxUsedTabNodeId());
  session_id.set_id(2);
  pool_.AddTabNode(1, session_id);
  EXPECT_EQ(10, GetMaxUsedTabNodeId());
  session_id.set_id(3);
  pool_.AddTabNode(1000, session_id);
  EXPECT_EQ(1000, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(1000, 500);

  // Freeing a tab node does not change max_used_tab_node_id_.
  pool_.FreeTabNode(1000);
  pool_.FreeUnassociatedTabNodes();
  EXPECT_EQ(1000, GetMaxUsedTabNodeId());
  for (int i = 0; i < 3; ++i) {
    pool_.AssociateTabNode(pool_.GetFreeTabNode(), i + 1);
    EXPECT_EQ(1000, GetMaxUsedTabNodeId());
  }

  EXPECT_EQ(1000, GetMaxUsedTabNodeId());
}

TEST_F(SyncTabNodePoolTest, OldTabNodesAddAndRemove) {
  // VerifyOldTabNodes are added.
  // tab_node_id = 1, tab_id = 1
  SessionID session_id;
  session_id.set_id(1);
  pool_.AddTabNode(1, session_id);
  // tab_node_id = 2, tab_id = 2
  session_id.set_id(2);
  pool_.AddTabNode(2, session_id);
  EXPECT_EQ(2u, pool_.Capacity());
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.IsUnassociatedTabNode(1));
  pool_.ReassociateTabNode(1, 2);
  EXPECT_TRUE(pool_.Empty());
  // Check FreeUnassociatedTabNodes returns the node to free node pool_.
  pool_.FreeUnassociatedTabNodes();
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
  SessionID session_id;
  session_id.set_id(1);
  pool_.AddTabNode(4, session_id);
  session_id.set_id(2);
  pool_.AddTabNode(5, session_id);
  session_id.set_id(3);
  pool_.AddTabNode(6, session_id);
  EXPECT_EQ(3u, pool_.Capacity());
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.IsUnassociatedTabNode(4));
  pool_.ReassociateTabNode(4, 5);
  // Free 5 and 6.
  pool_.FreeUnassociatedTabNodes();
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
  SessionID session_id;
  session_id.set_id(1);
  pool_.AddTabNode(5, session_id);
  session_id.set_id(2);
  pool_.AddTabNode(10, session_id);
  pool_.FreeUnassociatedTabNodes();
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
  SessionID session_id;
  session_id.set_id(1);
  pool_.AddTabNode(5, session_id);
  session_id.set_id(2);
  pool_.AddTabNode(10, session_id);
  // Free added nodes.
  pool_.FreeUnassociatedTabNodes();
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
