// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_priority_tree.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace test {

template <typename NodeId>
class SpdyPriorityTreePeer {
 public:
  explicit SpdyPriorityTreePeer(SpdyPriorityTree<NodeId>* tree) : tree_(tree) {}

  void PropagateNodeState(NodeId node) {
    tree_->PropagateNodeState(node);
  }

 private:
  SpdyPriorityTree<NodeId>* tree_;
};

TEST(SpdyPriorityTreeTest, AddAndRemoveNodes) {
  SpdyPriorityTree<uint32> tree;
  EXPECT_EQ(1, tree.num_nodes());
  EXPECT_TRUE(tree.NodeExists(0));
  EXPECT_FALSE(tree.NodeExists(1));

  EXPECT_TRUE(tree.AddNode(1, 0, 100, false));
  EXPECT_EQ(2, tree.num_nodes());
  ASSERT_TRUE(tree.NodeExists(1));
  EXPECT_EQ(100, tree.GetWeight(1));
  EXPECT_FALSE(tree.NodeExists(5));

  EXPECT_TRUE(tree.AddNode(5, 0, 50, false));
  // Should not be able to add a node with an id that already exists.
  EXPECT_FALSE(tree.AddNode(5, 1, 50, false));
  EXPECT_EQ(3, tree.num_nodes());
  EXPECT_TRUE(tree.NodeExists(1));
  ASSERT_TRUE(tree.NodeExists(5));
  EXPECT_EQ(50, tree.GetWeight(5));
  EXPECT_FALSE(tree.NodeExists(13));

  EXPECT_TRUE(tree.AddNode(13, 5, 130, true));
  EXPECT_EQ(4, tree.num_nodes());
  EXPECT_TRUE(tree.NodeExists(1));
  EXPECT_TRUE(tree.NodeExists(5));
  ASSERT_TRUE(tree.NodeExists(13));
  EXPECT_EQ(130, tree.GetWeight(13));
  EXPECT_EQ(5u, tree.GetParent(13));

  EXPECT_TRUE(tree.RemoveNode(5));
  // Cannot remove a node that has already been removed.
  EXPECT_FALSE(tree.RemoveNode(5));
  EXPECT_EQ(3, tree.num_nodes());
  EXPECT_TRUE(tree.NodeExists(1));
  EXPECT_FALSE(tree.NodeExists(5));
  EXPECT_TRUE(tree.NodeExists(13));
  EXPECT_EQ(0u, tree.GetParent(13));

  // The parent node 19 doesn't exist, so this should fail:
  EXPECT_FALSE(tree.AddNode(7, 19, 70, false));
  // This should succeed, creating node 7:
  EXPECT_TRUE(tree.AddNode(7, 13, 70, false));
  // Now node 7 already exists, so this should fail:
  EXPECT_FALSE(tree.AddNode(7, 1, 70, false));
  // Try adding a second child to node 13:
  EXPECT_TRUE(tree.AddNode(17, 13, 170, false));

  ASSERT_TRUE(tree.ValidateInvariantsForTests());
}

TEST(SpdyPriorityTreeTest, SetParent) {
  SpdyPriorityTree<uint32> tree;
  tree.AddNode(1, 0, 100, false);
  tree.AddNode(2, 1, 20, false);
  tree.AddNode(3, 2, 30, false);
  tree.AddNode(5, 3, 50, false);
  tree.AddNode(7, 5, 70, false);
  tree.AddNode(9, 7, 90, false);
  tree.AddNode(11, 0, 200, false);
  tree.AddNode(13, 11, 130, false);
  // We can't set the parent of a nonexistent node, or set the parent of an
  // existing node to a nonexistent node.
  EXPECT_FALSE(tree.SetParent(99, 13, false));
  EXPECT_FALSE(tree.SetParent(5, 99, false));
  // We should be able to add multiple children to nodes.
  EXPECT_TRUE(tree.SetParent(13, 7, false));
  EXPECT_TRUE(tree.SetParent(3, 11, false));
  // We should adjust the tree if a new dependency would create a cycle.
  EXPECT_TRUE(tree.SetParent(11, 13, false));
  EXPECT_TRUE(tree.SetParent(1, 9, false));
  EXPECT_TRUE(tree.SetParent(3, 9, false));
  // Test dependency changes.
  EXPECT_TRUE(tree.HasChild(5, 7));
  EXPECT_TRUE(tree.SetParent(7, 13, false));
  EXPECT_EQ(13u, tree.GetParent(7));
  EXPECT_TRUE(tree.HasChild(13, 7));

  EXPECT_TRUE(tree.SetParent(1, 9, false));
  EXPECT_EQ(9u, tree.GetParent(1));
  EXPECT_TRUE(tree.HasChild(9, 1));
  // Setting the parent of a node to its same parent should be a no-op.
  EXPECT_EQ(1u, tree.GetParent(2));
  EXPECT_TRUE(tree.HasChild(1, 2));
  EXPECT_TRUE(tree.SetParent(2, 1, true));
  EXPECT_EQ(1u, tree.GetParent(2));
  EXPECT_TRUE(tree.HasChild(1, 2));

  ASSERT_TRUE(tree.ValidateInvariantsForTests());
}

TEST(SpdyPriorityTreeTest, BlockAndUnblock) {
  /* Create the tree.

           0
          /|\
         1 2 3
        /| |  \
       4 5 6   7
      /| |\ \  |\
     8 91011121314
    / \
   15 16

  */
  SpdyPriorityTree<uint32> tree;
  SpdyPriorityTreePeer<uint32> tree_peer(&tree);
  tree.AddNode(1, 0, 100, false);
  tree.AddNode(2, 0, 100, false);
  tree.AddNode(3, 0, 100, false);
  tree.AddNode(4, 1, 100, false);
  tree.AddNode(5, 1, 100, false);
  tree.AddNode(8, 4, 100, false);
  tree.AddNode(9, 4, 100, false);
  tree.AddNode(10, 5, 100, false);
  tree.AddNode(11, 5, 100, false);
  tree.AddNode(15, 8, 100, false);
  tree.AddNode(16, 8, 100, false);
  tree.AddNode(12, 2, 100, false);
  tree.AddNode(6, 2, 100, true);
  tree.AddNode(7, 0, 100, false);
  tree.AddNode(13, 7, 100, true);
  tree.AddNode(14, 7, 100, false);
  tree.SetParent(7, 3, false);
  EXPECT_EQ(0u, tree.GetParent(1));
  EXPECT_EQ(0u, tree.GetParent(2));
  EXPECT_EQ(0u, tree.GetParent(3));
  EXPECT_EQ(1u, tree.GetParent(4));
  EXPECT_EQ(1u, tree.GetParent(5));
  EXPECT_EQ(2u, tree.GetParent(6));
  EXPECT_EQ(3u, tree.GetParent(7));
  EXPECT_EQ(4u, tree.GetParent(8));
  EXPECT_EQ(4u, tree.GetParent(9));
  EXPECT_EQ(5u, tree.GetParent(10));
  EXPECT_EQ(5u, tree.GetParent(11));
  EXPECT_EQ(6u, tree.GetParent(12));
  EXPECT_EQ(7u, tree.GetParent(13));
  EXPECT_EQ(7u, tree.GetParent(14));
  EXPECT_EQ(8u, tree.GetParent(15));
  EXPECT_EQ(8u, tree.GetParent(16));
  ASSERT_TRUE(tree.ValidateInvariantsForTests());

  EXPECT_EQ(tree.FindNode(0)->total_child_weights,
            tree.FindNode(1)->weight +
            tree.FindNode(2)->weight +
            tree.FindNode(3)->weight);
  EXPECT_EQ(tree.FindNode(3)->total_child_weights,
            tree.FindNode(7)->weight);
  EXPECT_EQ(tree.FindNode(7)->total_child_weights,
            tree.FindNode(13)->weight + tree.FindNode(14)->weight);
  EXPECT_EQ(tree.FindNode(13)->total_child_weights, 0);
  EXPECT_EQ(tree.FindNode(14)->total_child_weights, 0);

  // Set all nodes ready to write.
  EXPECT_TRUE(tree.SetReady(1, true));
  EXPECT_TRUE(tree.SetReady(2, true));
  EXPECT_TRUE(tree.SetReady(3, true));
  EXPECT_TRUE(tree.SetReady(4, true));
  EXPECT_TRUE(tree.SetReady(5, true));
  EXPECT_TRUE(tree.SetReady(6, true));
  EXPECT_TRUE(tree.SetReady(7, true));
  EXPECT_TRUE(tree.SetReady(8, true));
  EXPECT_TRUE(tree.SetReady(9, true));
  EXPECT_TRUE(tree.SetReady(10, true));
  EXPECT_TRUE(tree.SetReady(11, true));
  EXPECT_TRUE(tree.SetReady(12, true));
  EXPECT_TRUE(tree.SetReady(13, true));
  EXPECT_TRUE(tree.SetReady(14, true));
  EXPECT_TRUE(tree.SetReady(15, true));
  EXPECT_TRUE(tree.SetReady(16, true));

  // Number of readable child weights should not change because
  // 7 has unblocked children.
  tree.SetBlocked(7, true);
  tree_peer.PropagateNodeState(kRootNodeId);
  EXPECT_EQ(tree.FindNode(3)->total_child_weights,
            tree.FindNode(3)->total_writeable_child_weights);

  // Readable children for 7 should decrement.
  // Number of readable child weights for 3 still should not change.
  tree.SetBlocked(13, true);
  tree_peer.PropagateNodeState(kRootNodeId);
  EXPECT_EQ(tree.FindNode(3)->total_child_weights,
            tree.FindNode(3)->total_writeable_child_weights);
  EXPECT_EQ(tree.FindNode(14)->weight,
            tree.FindNode(7)->total_writeable_child_weights);

  // Once 14 becomes blocked, readable children for 7 and 3 should both be
  // decremented. Total readable weights at the root should still be the same
  // because 3 is still writeable.
  tree.SetBlocked(14, true);
  tree_peer.PropagateNodeState(kRootNodeId);
  EXPECT_EQ(0, tree.FindNode(3)->total_writeable_child_weights);
  EXPECT_EQ(0, tree.FindNode(7)->total_writeable_child_weights);
  EXPECT_EQ(tree.FindNode(0)->total_child_weights,
            tree.FindNode(1)->weight +
            tree.FindNode(2)->weight +
            tree.FindNode(3)->weight);

  // And now the root should be decremented as well.
  tree.SetBlocked(3, true);
  tree_peer.PropagateNodeState(kRootNodeId);
  EXPECT_EQ(tree.FindNode(1)->weight + tree.FindNode(2)->weight,
            tree.FindNode(0)->total_writeable_child_weights);

  // Unblocking 7 should propagate all the way up to the root.
  tree.SetBlocked(7, false);
  tree_peer.PropagateNodeState(kRootNodeId);
  EXPECT_EQ(tree.FindNode(0)->total_writeable_child_weights,
            tree.FindNode(1)->weight +
            tree.FindNode(2)->weight +
            tree.FindNode(3)->weight);
  EXPECT_EQ(tree.FindNode(3)->total_writeable_child_weights,
            tree.FindNode(7)->weight);
  EXPECT_EQ(0, tree.FindNode(7)->total_writeable_child_weights);

  // Ditto for reblocking 7.
  tree.SetBlocked(7, true);
  tree_peer.PropagateNodeState(kRootNodeId);
  EXPECT_EQ(tree.FindNode(0)->total_writeable_child_weights,
            tree.FindNode(1)->weight + tree.FindNode(2)->weight);
  EXPECT_EQ(0, tree.FindNode(3)->total_writeable_child_weights);
  EXPECT_EQ(0, tree.FindNode(7)->total_writeable_child_weights);
  ASSERT_TRUE(tree.ValidateInvariantsForTests());
}

TEST(SpdyPriorityTreeTest, GetPriorityList) {
  typedef uint32 SpdyStreamId;
  typedef std::pair<SpdyStreamId, float> PriorityNode;
  typedef std::vector<PriorityNode> PriorityList;

  PriorityList expected_list;
  PriorityList priority_list;

  /* Create the tree.

           0
          /|\
         1 2 3
        /| |\
       4 5 6 7
      /
     8

  */
  SpdyPriorityTree<uint32> tree;
  tree.AddNode(1, 0, 10, false);
  tree.AddNode(2, 0, 20, false);
  tree.AddNode(3, 0, 30, false);
  tree.AddNode(4, 1, 10, false);
  tree.AddNode(5, 1, 90, false);
  tree.AddNode(6, 2, 10, false);
  tree.AddNode(7, 2, 10, false);
  tree.AddNode(8, 4, 256, false);

  // Set all nodes ready to write.
  EXPECT_TRUE(tree.SetReady(1, true));
  EXPECT_TRUE(tree.SetReady(2, true));
  EXPECT_TRUE(tree.SetReady(3, true));
  EXPECT_TRUE(tree.SetReady(4, true));
  EXPECT_TRUE(tree.SetReady(5, true));
  EXPECT_TRUE(tree.SetReady(6, true));
  EXPECT_TRUE(tree.SetReady(7, true));
  EXPECT_TRUE(tree.SetReady(8, true));

  expected_list.push_back(PriorityNode(3, 1.0/2.0));
  expected_list.push_back(PriorityNode(2, 1.0/3.0));
  expected_list.push_back(PriorityNode(1, 1.0/6.0));
  priority_list = tree.GetPriorityList();
  EXPECT_EQ(expected_list, priority_list);

  // Check that the list updates as expected when a node gets blocked.
  EXPECT_TRUE(tree.SetReady(1, false));
  expected_list.clear();
  expected_list.push_back(PriorityNode(3, 1.0/2.0));
  expected_list.push_back(PriorityNode(2, 1.0/3.0));
  expected_list.push_back(PriorityNode(5, 0.9*1.0/6.0));
  expected_list.push_back(PriorityNode(4, 0.1*1.0/6.0));
  priority_list = tree.GetPriorityList();
  EXPECT_EQ(expected_list, priority_list);

  // Block multiple levels of nodes.
  EXPECT_TRUE(tree.SetReady(4, false));
  EXPECT_TRUE(tree.SetReady(5, false));
  expected_list.clear();
  expected_list.push_back(PriorityNode(3, 1.0/2.0));
  expected_list.push_back(PriorityNode(2, 1.0/3.0));
  expected_list.push_back(PriorityNode(8, 1.0/6.0));
  priority_list = tree.GetPriorityList();
  EXPECT_EQ(expected_list, priority_list);

  // Remove a node from the tree to make sure priorities
  // get redistributed accordingly.
  EXPECT_TRUE(tree.RemoveNode(1));
  expected_list.clear();
  expected_list.push_back(PriorityNode(3, 30.0/51.0));
  expected_list.push_back(PriorityNode(2, 20.0/51.0));
  expected_list.push_back(PriorityNode(8, 1.0/51.0));
  priority_list = tree.GetPriorityList();
  EXPECT_EQ(expected_list, priority_list);

  // Block an entire subtree.
  EXPECT_TRUE(tree.SetReady(8, false));
  expected_list.clear();
  expected_list.push_back(PriorityNode(3, 0.6));
  expected_list.push_back(PriorityNode(2, 0.4));
  priority_list = tree.GetPriorityList();
  EXPECT_EQ(expected_list, priority_list);

  // Unblock previously blocked nodes.
  EXPECT_TRUE(tree.SetReady(4, true));
  EXPECT_TRUE(tree.SetReady(5, true));
  expected_list.clear();
  expected_list.push_back(PriorityNode(3, 1.0/2.0));
  expected_list.push_back(PriorityNode(2, 1.0/3.0));
  expected_list.push_back(PriorityNode(5, 9.0/60.0));
  expected_list.push_back(PriorityNode(4, 1.0/60.0));
  priority_list = tree.GetPriorityList();
  EXPECT_EQ(expected_list, priority_list);

  // Blocked nodes in multiple subtrees.
  EXPECT_TRUE(tree.SetReady(2, false));
  EXPECT_TRUE(tree.SetReady(6, false));
  EXPECT_TRUE(tree.SetReady(7, false));
  expected_list.clear();
  expected_list.push_back(PriorityNode(3, 3.0/4.0));
  expected_list.push_back(PriorityNode(5, 9.0/40.0));
  expected_list.push_back(PriorityNode(4, 1.0/40.0));
  priority_list = tree.GetPriorityList();
  EXPECT_EQ(expected_list, priority_list);
}

TEST(SpdyPriorityTreeTest, CalculateRoundedWeights) {
  typedef uint32 SpdyStreamId;
  typedef std::pair<SpdyStreamId, float> PriorityNode;
  typedef std::vector<PriorityNode> PriorityList;

  PriorityList expected_list;
  PriorityList priority_list;

  /* Create the tree.

           0
          / \
         1   2
       //|\  |\
      83 4 5 6 7
  */
  SpdyPriorityTree<uint32> tree;
  tree.AddNode(3, 0, 100, false);
  tree.AddNode(4, 0, 100, false);
  tree.AddNode(5, 0, 100, false);
  tree.AddNode(1, 0, 10, true);
  tree.AddNode(2, 0, 5, false);
  tree.AddNode(6, 2, 1, false);
  tree.AddNode(7, 2, 1, false);
  tree.AddNode(8, 1, 1, false);

  // Remove higher-level nodes.
  tree.RemoveNode(1);
  tree.RemoveNode(2);

  // 3.3 rounded down = 3.
  EXPECT_EQ(3, tree.GetWeight(3));
  EXPECT_EQ(3, tree.GetWeight(4));
  EXPECT_EQ(3, tree.GetWeight(5));
  // 2.5 rounded up = 3.
  EXPECT_EQ(3, tree.GetWeight(6));
  EXPECT_EQ(3, tree.GetWeight(7));
  // 0 is not a valid weight, so round up to 1.
  EXPECT_EQ(1, tree.GetWeight(8));
}
}  // namespace test
}  // namespace gfe_spdy
