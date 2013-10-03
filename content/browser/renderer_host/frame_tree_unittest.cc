// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree.h"

#include "base/run_loop.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class FrameTreeTest : public RenderViewHostTestHarness {
};

// The root node never changes during navigation even though its
// RenderFrameHost does.
//  - Swapping main frame doesn't change root node.
//  - Swapping back to NULL doesn't crash (easier tear-down for interstitials).
//  - Main frame does not own RenderFrameHost.
TEST_F(FrameTreeTest, RootNode) {
  FrameTree frame_tree;

  // Initial state has empty node.
  FrameTreeNode* root = frame_tree.GetRootForTesting();
  ASSERT_TRUE(root);
  EXPECT_FALSE(frame_tree.GetMainFrame());

  // Swap in main frame.
  RenderFrameHostImpl* dummy = reinterpret_cast<RenderFrameHostImpl*>(0x1);
  frame_tree.SwapMainFrame(dummy);
  EXPECT_EQ(root, frame_tree.GetRootForTesting());
  EXPECT_EQ(dummy, frame_tree.GetMainFrame());

  // Move back to NULL.
  frame_tree.SwapMainFrame(NULL);
  EXPECT_EQ(root, frame_tree.GetRootForTesting());
  EXPECT_FALSE(frame_tree.GetMainFrame());

  // Move back to an invalid pointer, let the FrameTree go out of scope. Test
  // should not crash because the main frame isn't owned.
  frame_tree.SwapMainFrame(dummy);
}

// Test that swapping the main frame resets the renderer-assigned frame id.
//  - On creation, frame id is unassigned.
//  - After a swap, frame id is unassigned.
TEST_F(FrameTreeTest, FirstNavigationAfterSwap) {
  FrameTree frame_tree;

  EXPECT_TRUE(frame_tree.IsFirstNavigationAfterSwap());
  EXPECT_EQ(FrameTreeNode::kInvalidFrameId,
            frame_tree.GetRootForTesting()->frame_id());
  frame_tree.OnFirstNavigationAfterSwap(1);
  EXPECT_FALSE(frame_tree.IsFirstNavigationAfterSwap());
  EXPECT_EQ(1, frame_tree.GetRootForTesting()->frame_id());

  frame_tree.SwapMainFrame(NULL);
  EXPECT_TRUE(frame_tree.IsFirstNavigationAfterSwap());
  EXPECT_EQ(FrameTreeNode::kInvalidFrameId,
            frame_tree.GetRootForTesting()->frame_id());
}

// Exercise tree manipulation routines.
//  - Add a series of nodes and verify tree structure.
//  - Remove a series of nodes and verify tree structure.
TEST_F(FrameTreeTest, Shape) {
  FrameTree frame_tree;
  std::string no_children_node("no children node");
  std::string deep_subtree("node with deep subtree");

  // Ensure the top-level node of the FrameTree is initialized by simulating a
  // main frame swap here.
  RenderFrameHostImpl render_frame_host(static_cast<RenderViewHostImpl*>(rvh()),
                                        &frame_tree,
                                        process()->GetNextRoutingID(), false);
  frame_tree.SwapMainFrame(&render_frame_host);
  frame_tree.OnFirstNavigationAfterSwap(5);

  // Simulate attaching a series of frames to build the frame tree.
  frame_tree.AddFrame(process()->GetNextRoutingID(), 5, 14, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 5, 15, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 5, 16, std::string());

  frame_tree.AddFrame(process()->GetNextRoutingID(), 14, 244, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 14, 245, std::string());

  frame_tree.AddFrame(process()->GetNextRoutingID(), 15, 255, no_children_node);

  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 264, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 265, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 266, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 267, deep_subtree);
  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 268, std::string());

  frame_tree.AddFrame(process()->GetNextRoutingID(), 267, 365, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 365, 455, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 455, 555, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 555, 655, std::string());

  // Now, verify the tree structure is as expected.
  FrameTreeNode* root = frame_tree.GetRootForTesting();
  EXPECT_EQ(5, root->frame_id());
  EXPECT_EQ(3UL, root->child_count());

  EXPECT_EQ(2UL, root->child_at(0)->child_count());
  EXPECT_EQ(0UL, root->child_at(0)->child_at(0)->child_count());
  EXPECT_EQ(0UL, root->child_at(0)->child_at(1)->child_count());

  EXPECT_EQ(1UL, root->child_at(1)->child_count());
  EXPECT_EQ(0UL, root->child_at(1)->child_at(0)->child_count());
  EXPECT_STREQ(no_children_node.c_str(),
      root->child_at(1)->child_at(0)->frame_name().c_str());

  EXPECT_EQ(5UL, root->child_at(2)->child_count());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(0)->child_count());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(1)->child_count());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(2)->child_count());
  EXPECT_EQ(1UL, root->child_at(2)->child_at(3)->child_count());
  EXPECT_STREQ(deep_subtree.c_str(),
      root->child_at(2)->child_at(3)->frame_name().c_str());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(4)->child_count());

  FrameTreeNode* deep_tree = root->child_at(2)->child_at(3)->child_at(0);
  EXPECT_EQ(365, deep_tree->frame_id());
  EXPECT_EQ(1UL, deep_tree->child_count());
  EXPECT_EQ(455, deep_tree->child_at(0)->frame_id());
  EXPECT_EQ(1UL, deep_tree->child_at(0)->child_count());
  EXPECT_EQ(555, deep_tree->child_at(0)->child_at(0)->frame_id());
  EXPECT_EQ(1UL, deep_tree->child_at(0)->child_at(0)->child_count());
  EXPECT_EQ(655, deep_tree->child_at(0)->child_at(0)->child_at(0)->frame_id());
  EXPECT_EQ(0UL,
      deep_tree->child_at(0)->child_at(0)->child_at(0)->child_count());

  // Test removing of nodes.
  frame_tree.RemoveFrame(555, 655);
  EXPECT_EQ(0UL, deep_tree->child_at(0)->child_at(0)->child_count());

  frame_tree.RemoveFrame(16, 265);
  EXPECT_EQ(4UL, root->child_at(2)->child_count());

  frame_tree.RemoveFrame(5, 15);
  EXPECT_EQ(2UL, root->child_count());
}

}  // namespace
}  // namespace content
