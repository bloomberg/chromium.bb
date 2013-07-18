// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/tab_node_pool.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

class SyncTabNodePoolTest : public testing::Test {
 protected:
  SyncTabNodePoolTest() : pool_(NULL) { pool_.SetMachineTag("tag"); }

  size_t GetMaxUsedTabNodeId() const { return pool_.max_used_tab_node_id_; }

  TabNodePool pool_;
};

namespace {

TEST_F(SyncTabNodePoolTest, TabNodeIdIncreases) {
  // max_used_tab_node_ always increases.
  SessionID session_id;
  session_id.set_id(1);
  pool_.AddTabNode(4, session_id, 10);
  EXPECT_EQ(10u, GetMaxUsedTabNodeId());
  session_id.set_id(2);
  pool_.AddTabNode(5, session_id, 1);
  EXPECT_EQ(10u, GetMaxUsedTabNodeId());
  session_id.set_id(3);
  pool_.AddTabNode(6, session_id, 1000);
  EXPECT_EQ(1000u, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(6, 500);
  // Freeing a tab node does not change max_used_tab_node_id_.
  pool_.FreeTabNode(6);
  pool_.FreeUnassociatedTabNodes();
  EXPECT_EQ(1000u, GetMaxUsedTabNodeId());
  for (int i = 0; i < 3; ++i) {
    pool_.AssociateTabNode(pool_.GetFreeTabNode(), i + 1);
    EXPECT_EQ(1000u, GetMaxUsedTabNodeId());
  }

  EXPECT_EQ(1000u, GetMaxUsedTabNodeId());
}

TEST_F(SyncTabNodePoolTest, OldTabNodesAddAndRemove) {
  // VerifyOldTabNodes are added.
  // sync_id =4, tab_node_id = 1, tab_id = 1
  SessionID session_id;
  session_id.set_id(1);
  pool_.AddTabNode(4, session_id, 1);
  // sync_id = 5, tab_node_id = 5, tab_id = 2
  session_id.set_id(2);
  pool_.AddTabNode(5, session_id, 2);
  EXPECT_EQ(2u, pool_.Capacity());
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.IsUnassociatedTabNode(4));
  pool_.ReassociateTabNode(4, 2);
  EXPECT_TRUE(pool_.Empty());
  // Check FreeUnassociatedTabNodes returns the node to free node pool_.
  pool_.FreeUnassociatedTabNodes();
  // 5 should be returned to free node pool_.
  EXPECT_EQ(2u, pool_.Capacity());
  // Should be able to free 4.
  pool_.FreeTabNode(4);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(4, pool_.GetFreeTabNode());
  pool_.AssociateTabNode(4, 1);
  EXPECT_EQ(5, pool_.GetFreeTabNode());
  pool_.AssociateTabNode(5, 1);
  EXPECT_TRUE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
}

TEST_F(SyncTabNodePoolTest, OldTabNodesReassociation) {
  // VerifyOldTabNodes are reassociated correctly.
  // sync_id =4, tab_node_id = 1, tab_id = 1
  SessionID session_id;
  session_id.set_id(1);
  pool_.AddTabNode(4, session_id, 1);
  // sync_id = 5, tab_node_id = 2, tab_id = 2
  session_id.set_id(2);
  pool_.AddTabNode(5, session_id, 2);
  // sync_id = 6, tab_node_id = 3, tab_id =3
  session_id.set_id(3);
  pool_.AddTabNode(6, session_id, 3);
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
  std::set<int64> free_sync_ids;
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
  pool_.AddTabNode(5, session_id, 1);
  session_id.set_id(2);
  pool_.AddTabNode(10, session_id, 2);
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
  pool_.AddTabNode(5, session_id, 1);
  session_id.set_id(2);
  pool_.AddTabNode(10, session_id, 2);
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
