// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions2/tab_node_pool2.h"

#include "sync/api/sync_change.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

class SyncTabNodePool2Test : public testing::Test {
 protected:
  SyncTabNodePool2Test() { pool_.SetMachineTag("tag"); }

  int GetMaxUsedTabNodeId() const { return pool_.max_used_tab_node_id_; }

  TabNodePool2 pool_;
};

namespace {

TEST_F(SyncTabNodePool2Test, TabNodeIdIncreases) {
  syncer::SyncChangeList changes;
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
  pool_.FreeTabNode(1000, &changes);
  EXPECT_TRUE(changes.empty());
  pool_.FreeUnassociatedTabNodes(&changes);
  EXPECT_TRUE(changes.empty());
  EXPECT_EQ(1000, GetMaxUsedTabNodeId());
  for (int i = 0; i < 3; ++i) {
    pool_.AssociateTabNode(pool_.GetFreeTabNode(&changes), i + 1);
    EXPECT_EQ(1000, GetMaxUsedTabNodeId());
  }
  // We Free() nodes above, so GetFreeTabNode should not need to create.
  EXPECT_TRUE(changes.empty());

  EXPECT_EQ(1000, GetMaxUsedTabNodeId());
}

TEST_F(SyncTabNodePool2Test, OldTabNodesAddAndRemove) {
  syncer::SyncChangeList changes;
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
  pool_.FreeUnassociatedTabNodes(&changes);
  EXPECT_TRUE(changes.empty());
  // 2 should be returned to free node pool_.
  EXPECT_EQ(2u, pool_.Capacity());
  // Should be able to free 1.
  pool_.FreeTabNode(1, &changes);
  EXPECT_TRUE(changes.empty());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(1, pool_.GetFreeTabNode(&changes));
  EXPECT_TRUE(changes.empty());
  pool_.AssociateTabNode(1, 1);
  EXPECT_EQ(2, pool_.GetFreeTabNode(&changes));
  EXPECT_TRUE(changes.empty());
  pool_.AssociateTabNode(2, 1);
  EXPECT_TRUE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
}

TEST_F(SyncTabNodePool2Test, OldTabNodesReassociation) {
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
  syncer::SyncChangeList changes;
  pool_.FreeUnassociatedTabNodes(&changes);
  EXPECT_TRUE(changes.empty());
  // 5 and 6 nodes should not be unassociated.
  EXPECT_FALSE(pool_.IsUnassociatedTabNode(5));
  EXPECT_FALSE(pool_.IsUnassociatedTabNode(6));
  // Free node pool should have 5 and 6.
  EXPECT_FALSE(pool_.Empty());
  EXPECT_EQ(3u, pool_.Capacity());

  // Free all nodes
  pool_.FreeTabNode(4, &changes);
  EXPECT_TRUE(changes.empty());
  EXPECT_TRUE(pool_.Full());
  std::set<int> free_sync_ids;
  for (int i = 0; i < 3; ++i) {
    free_sync_ids.insert(pool_.GetFreeTabNode(&changes));
    // GetFreeTabNode will return the same value till the node is
    // reassociated.
    pool_.AssociateTabNode(pool_.GetFreeTabNode(&changes), i + 1);
  }

  EXPECT_TRUE(pool_.Empty());
  EXPECT_EQ(3u, free_sync_ids.size());
  EXPECT_EQ(1u, free_sync_ids.count(4));
  EXPECT_EQ(1u, free_sync_ids.count(5));
  EXPECT_EQ(1u, free_sync_ids.count(6));
}

TEST_F(SyncTabNodePool2Test, Init) {
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
}

TEST_F(SyncTabNodePool2Test, AddGet) {
  syncer::SyncChangeList changes;
  SessionID session_id;
  session_id.set_id(1);
  pool_.AddTabNode(5, session_id);
  session_id.set_id(2);
  pool_.AddTabNode(10, session_id);
  pool_.FreeUnassociatedTabNodes(&changes);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());

  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_EQ(5, pool_.GetFreeTabNode(&changes));
  pool_.AssociateTabNode(5, 1);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  // 5 is now used, should return 10.
  EXPECT_EQ(10, pool_.GetFreeTabNode(&changes));
}

TEST_F(SyncTabNodePool2Test, All) {
  syncer::SyncChangeList changes;
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(0U, pool_.Capacity());

  // GetFreeTabNode returns the lowest numbered free node.
  EXPECT_EQ(0, pool_.GetFreeTabNode(&changes));
  EXPECT_EQ(1U, changes.size());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(1U, pool_.Capacity());

  // Associate 5, next free node should be 10.
  pool_.AssociateTabNode(0, 1);
  EXPECT_EQ(1, pool_.GetFreeTabNode(&changes));
  EXPECT_EQ(2U, changes.size());
  changes.clear();
  pool_.AssociateTabNode(1, 2);
  EXPECT_TRUE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  // Release them in reverse order.
  pool_.FreeTabNode(1, &changes);
  pool_.FreeTabNode(0, &changes);
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(0, pool_.GetFreeTabNode(&changes));
  EXPECT_TRUE(changes.empty());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  pool_.AssociateTabNode(0, 1);
  EXPECT_EQ(2U, pool_.Capacity());
  EXPECT_EQ(1, pool_.GetFreeTabNode(&changes));
  EXPECT_TRUE(changes.empty());
  pool_.AssociateTabNode(1, 2);
  EXPECT_TRUE(pool_.Empty());
  EXPECT_FALSE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  // Release them again.
  pool_.FreeTabNode(1, &changes);
  pool_.FreeTabNode(0, &changes);
  EXPECT_FALSE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(2U, pool_.Capacity());
  pool_.Clear();
  EXPECT_TRUE(pool_.Empty());
  EXPECT_TRUE(pool_.Full());
  EXPECT_EQ(0U, pool_.Capacity());
}

TEST_F(SyncTabNodePool2Test, GetFreeTabNodeCreate) {
  syncer::SyncChangeList changes;
  EXPECT_EQ(0, pool_.GetFreeTabNode(&changes));
  EXPECT_TRUE(changes[0].IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());
  EXPECT_TRUE(changes[0].sync_data().IsValid());
  sync_pb::EntitySpecifics entity = changes[0].sync_data().GetSpecifics();
  sync_pb::SessionSpecifics specifics(entity.session());
  EXPECT_EQ(0, specifics.tab_node_id());
}

}  // namespace

}  // namespace browser_sync
