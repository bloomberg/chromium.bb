// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/tab_node_pool.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {

class SyncTabNodePoolTest : public testing::Test {
 protected:
  SyncTabNodePoolTest() {}

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

const int kTabNodeId1 = 10;
const int kTabNodeId2 = 5;
const int kTabNodeId3 = 1000;
const int kTabId1 = 1;
const int kTabId2 = 2;
const int kTabId3 = 3;

TEST_F(SyncTabNodePoolTest, TabNodeIdIncreases) {
  std::set<int> deleted_node_ids;

  // max_used_tab_node_ always increases.
  pool_.ReassociateTabNode(kTabNodeId1, kTabId1);
  EXPECT_EQ(kTabNodeId1, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(kTabNodeId2, kTabId2);
  EXPECT_EQ(kTabNodeId1, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(kTabNodeId3, kTabId3);
  EXPECT_EQ(kTabNodeId3, GetMaxUsedTabNodeId());
  // Freeing a tab node does not change max_used_tab_node_id_.
  pool_.FreeTab(kTabId3);
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_TRUE(deleted_node_ids.empty());
  pool_.FreeTab(kTabId2);
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_TRUE(deleted_node_ids.empty());
  pool_.FreeTab(kTabId1);
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_TRUE(deleted_node_ids.empty());
  for (int i = 0; i < 3; ++i) {
    int tab_node_id = -1;
    EXPECT_TRUE(pool_.GetTabNodeForTab(i + 1, &tab_node_id));
    EXPECT_EQ(kTabNodeId3, GetMaxUsedTabNodeId());
  }
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_TRUE(deleted_node_ids.empty());
  EXPECT_EQ(kTabNodeId3, GetMaxUsedTabNodeId());
  EXPECT_TRUE(pool_.Empty());
}

TEST_F(SyncTabNodePoolTest, Reassociation) {
  // Reassociate tab node 1 with tab id 1.
  pool_.ReassociateTabNode(kTabNodeId1, kTabId1);
  EXPECT_EQ(1U, pool_.Capacity());
  EXPECT_TRUE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(kTabId1, pool_.GetTabIdFromTabNodeId(kTabNodeId1));
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabIdFromTabNodeId(kTabNodeId2));

  // Introduce a new tab node associated with the same tab. The old tab node
  // should get added to the free pool
  pool_.ReassociateTabNode(kTabNodeId2, kTabId1);
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabIdFromTabNodeId(kTabNodeId1));
  EXPECT_EQ(kTabId1, pool_.GetTabIdFromTabNodeId(kTabNodeId2));

  // Reassociating the same tab node/tab should have no effect.
  pool_.ReassociateTabNode(kTabNodeId2, kTabId1);
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabIdFromTabNodeId(kTabNodeId1));
  EXPECT_EQ(kTabId1, pool_.GetTabIdFromTabNodeId(kTabNodeId2));

  // Reassociating the new tab node with a new tab should just update the
  // association tables.
  pool_.ReassociateTabNode(kTabNodeId2, kTabId2);
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabIdFromTabNodeId(kTabNodeId1));
  EXPECT_EQ(kTabId2, pool_.GetTabIdFromTabNodeId(kTabNodeId2));

  // Reassociating the first tab node should make the pool empty.
  pool_.ReassociateTabNode(kTabNodeId1, kTabId1);
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_TRUE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(kTabId1, pool_.GetTabIdFromTabNodeId(kTabNodeId1));
  EXPECT_EQ(kTabId2, pool_.GetTabIdFromTabNodeId(kTabNodeId2));
}

TEST_F(SyncTabNodePoolTest, ReassociateThenFree) {
  std::set<int> deleted_node_ids;

  // Verify old tab nodes are reassociated correctly.
  pool_.ReassociateTabNode(kTabNodeId1, kTabId1);
  pool_.ReassociateTabNode(kTabNodeId2, kTabId2);
  pool_.ReassociateTabNode(kTabNodeId3, kTabId3);
  EXPECT_EQ(3u, pool_.Capacity());
  EXPECT_TRUE(pool_.Empty());
  // Free tabs 2 and 3.
  pool_.FreeTab(kTabId2);
  pool_.FreeTab(kTabId3);
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_TRUE(deleted_node_ids.empty());
  // Free node pool should have 2 and 3.
  EXPECT_FALSE(pool_.Empty());
  EXPECT_EQ(3u, pool_.Capacity());

  // Free all nodes
  pool_.FreeTab(kTabId1);
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_TRUE(deleted_node_ids.empty());
  EXPECT_TRUE(pool_.Full());
  std::set<int> free_sync_ids;
  for (int i = 0; i < 3; ++i) {
    int tab_node_id = -1;
    EXPECT_TRUE(pool_.GetTabNodeForTab(i, &tab_node_id));
    free_sync_ids.insert(tab_node_id);
  }

  EXPECT_TRUE(pool_.Empty());
  EXPECT_EQ(3u, free_sync_ids.size());
  EXPECT_EQ(1u, free_sync_ids.count(kTabNodeId1));
  EXPECT_EQ(1u, free_sync_ids.count(kTabNodeId2));
  EXPECT_EQ(1u, free_sync_ids.count(kTabNodeId3));
}

TEST_F(SyncTabNodePoolTest, Init) {
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
}

TEST_F(SyncTabNodePoolTest, AddGet) {
  int free_nodes[] = {5, 10};
  AddFreeTabNodes(2, free_nodes);

  EXPECT_EQ(2U, pool_.Capacity());
  int tab_node_id = -1;
  EXPECT_TRUE(pool_.GetTabNodeForTab(1, &tab_node_id));
  EXPECT_EQ(5, tab_node_id);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  // 5 is now used, should return 10.
  EXPECT_TRUE(pool_.GetTabNodeForTab(2, &tab_node_id));
  EXPECT_EQ(10, tab_node_id);
}

TEST_F(SyncTabNodePoolTest, GetTabNodeForTabCreate) {
  int tab_node_id = -1;
  EXPECT_FALSE(pool_.GetTabNodeForTab(1, &tab_node_id));
  EXPECT_EQ(0, tab_node_id);
}

TEST_F(SyncTabNodePoolTest, TabPoolFreeNodeLimits) {
  std::set<int> deleted_node_ids;

  // Allocate TabNodePool::kFreeNodesHighWatermark + 1 nodes and verify that
  // freeing the last node reduces the free node pool size to
  // kFreeNodesLowWatermark.
  SessionID session_id;
  std::vector<int> used_sync_ids;
  for (size_t i = 1; i <= TabNodePool::kFreeNodesHighWatermark + 1; ++i) {
    session_id.set_id(i);
    int sync_id = -1;
    EXPECT_FALSE(pool_.GetTabNodeForTab(i, &sync_id));
    used_sync_ids.push_back(sync_id);
  }

  // Free all except one node.
  used_sync_ids.pop_back();

  for (size_t i = 1; i <= used_sync_ids.size(); ++i) {
    pool_.FreeTab(i);
    pool_.CleanupTabNodes(&deleted_node_ids);
    EXPECT_TRUE(deleted_node_ids.empty());
  }

  // Except one node all nodes should be in FreeNode pool.
  EXPECT_FALSE(pool_.Full());
  EXPECT_FALSE(pool_.Empty());
  // Total capacity = 1 Associated Node + kFreeNodesHighWatermark free node.
  EXPECT_EQ(TabNodePool::kFreeNodesHighWatermark + 1, pool_.Capacity());

  // Freeing the last sync node should drop the free nodes to
  // kFreeNodesLowWatermark.
  pool_.FreeTab(TabNodePool::kFreeNodesHighWatermark + 1);
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_EQ(TabNodePool::kFreeNodesHighWatermark + 1 -
                TabNodePool::kFreeNodesLowWatermark,
            deleted_node_ids.size());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(TabNodePool::kFreeNodesLowWatermark, pool_.Capacity());
}

}  // namespace

}  // namespace sync_sessions
