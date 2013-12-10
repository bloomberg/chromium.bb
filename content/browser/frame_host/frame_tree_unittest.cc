// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_factory.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class FrameTreeTest : public RenderViewHostTestHarness {
 protected:
  // Prints a FrameTree, for easy assertions of the tree hierarchy.
  std::string GetTreeState(FrameTree* frame_tree) {
    std::string result;
    AppendTreeNodeState(frame_tree->root(), &result);
    return result;
  }

 private:
  void AppendTreeNodeState(FrameTreeNode* node, std::string* result) {
    result->append(base::Int64ToString(node->frame_id()));
    if (!node->frame_name().empty()) {
      result->append(" '");
      result->append(node->frame_name());
      result->append("'");
    }
    result->append(": [");
    const char* separator = "";
    for (size_t i = 0; i < node->child_count(); i++) {
      result->append(separator);
      AppendTreeNodeState(node->child_at(i), result);
      separator = ", ";
    }
    result->append("]");
  }
};

// The root node never changes during navigation even though its
// RenderFrameHost does.
//  - Swapping main frame doesn't change root node.
//  - Swapping back to NULL doesn't crash (easier tear-down for interstitials).
//  - Main frame does not own RenderFrameHost.
TEST_F(FrameTreeTest, RootNode) {
  FrameTree frame_tree(new NavigatorImpl(NULL, NULL), NULL, NULL, NULL, NULL);

  // Initial state has empty node.
  FrameTreeNode* root = frame_tree.root();
  ASSERT_TRUE(root);
  EXPECT_FALSE(frame_tree.GetMainFrame());

  // Swap in main frame.
  RenderFrameHostImpl* dummy = reinterpret_cast<RenderFrameHostImpl*>(0x1);
  frame_tree.SwapMainFrame(dummy);
  EXPECT_EQ(root, frame_tree.root());
  EXPECT_EQ(dummy, frame_tree.GetMainFrame());

  // Move back to NULL.
  frame_tree.SwapMainFrame(NULL);
  EXPECT_EQ(root, frame_tree.root());
  EXPECT_FALSE(frame_tree.GetMainFrame());

  // Move back to an invalid pointer, let the FrameTree go out of scope. Test
  // should not crash because the main frame isn't owned.
  frame_tree.SwapMainFrame(dummy);
}

// Test that swapping the main frame resets the renderer-assigned frame id.
//  - On creation, frame id is unassigned.
//  - After a swap, frame id is unassigned.
TEST_F(FrameTreeTest, FirstNavigationAfterSwap) {
  FrameTree frame_tree(new NavigatorImpl(NULL, NULL), NULL, NULL, NULL, NULL);

  EXPECT_TRUE(frame_tree.IsFirstNavigationAfterSwap());
  EXPECT_EQ(FrameTreeNode::kInvalidFrameId,
            frame_tree.root()->frame_id());
  frame_tree.OnFirstNavigationAfterSwap(1);
  EXPECT_FALSE(frame_tree.IsFirstNavigationAfterSwap());
  EXPECT_EQ(1, frame_tree.root()->frame_id());

  frame_tree.SwapMainFrame(NULL);
  EXPECT_TRUE(frame_tree.IsFirstNavigationAfterSwap());
  EXPECT_EQ(FrameTreeNode::kInvalidFrameId,
            frame_tree.root()->frame_id());
}

// Exercise tree manipulation routines.
//  - Add a series of nodes and verify tree structure.
//  - Remove a series of nodes and verify tree structure.
TEST_F(FrameTreeTest, Shape) {
  FrameTree frame_tree(new NavigatorImpl(NULL, NULL), NULL, NULL, NULL, NULL);

  std::string no_children_node("no children node");
  std::string deep_subtree("node with deep subtree");

  // Ensure the top-level node of the FrameTree is initialized by simulating a
  // main frame swap here.
  scoped_ptr<RenderFrameHostImpl> render_frame_host =
      RenderFrameHostFactory::Create(static_cast<RenderViewHostImpl*>(rvh()),
                                     NULL,
                                     &frame_tree,
                                     frame_tree.root(),
                                     process()->GetNextRoutingID(),
                                     false);
  frame_tree.SwapMainFrame(render_frame_host.get());
  frame_tree.OnFirstNavigationAfterSwap(5);

  ASSERT_EQ("5: []", GetTreeState(&frame_tree));

  // Simulate attaching a series of frames to build the frame tree.
  frame_tree.AddFrame(process()->GetNextRoutingID(), 5, 14, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 5, 15, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 5, 16, std::string());

  frame_tree.AddFrame(process()->GetNextRoutingID(), 14, 244, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 15, 255, no_children_node);
  frame_tree.AddFrame(process()->GetNextRoutingID(), 14, 245, std::string());

  ASSERT_EQ("5: [14: [244: [], 245: []], "
                "15: [255 'no children node': []], "
                "16: []]",
            GetTreeState(&frame_tree));

  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 264, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 265, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 266, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 267, deep_subtree);
  frame_tree.AddFrame(process()->GetNextRoutingID(), 16, 268, std::string());

  frame_tree.AddFrame(process()->GetNextRoutingID(), 267, 365, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 365, 455, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 455, 555, std::string());
  frame_tree.AddFrame(process()->GetNextRoutingID(), 555, 655, std::string());

  // Now that's it's fully built, verify the tree structure is as expected.
  ASSERT_EQ("5: [14: [244: [], 245: []], "
                "15: [255 'no children node': []], "
                "16: [264: [], 265: [], 266: [], "
                     "267 'node with deep subtree': "
                         "[365: [455: [555: [655: []]]]], 268: []]]",
            GetTreeState(&frame_tree));

  // Test removing of nodes.
  frame_tree.RemoveFrame(NULL, 555, 655);
  ASSERT_EQ("5: [14: [244: [], 245: []], "
                "15: [255 'no children node': []], "
                "16: [264: [], 265: [], 266: [], "
                     "267 'node with deep subtree': "
                         "[365: [455: [555: []]]], 268: []]]",
            GetTreeState(&frame_tree));

  frame_tree.RemoveFrame(NULL, 16, 265);
  ASSERT_EQ("5: [14: [244: [], 245: []], "
                "15: [255 'no children node': []], "
                "16: [264: [], 266: [], "
                     "267 'node with deep subtree': "
                         "[365: [455: [555: []]]], 268: []]]",
            GetTreeState(&frame_tree));

  frame_tree.RemoveFrame(NULL, 5, 15);
  ASSERT_EQ("5: [14: [244: [], 245: []], "
                "16: [264: [], 266: [], "
                     "267 'node with deep subtree': "
                         "[365: [455: [555: []]]], 268: []]]",
            GetTreeState(&frame_tree));
}

}  // namespace
}  // namespace content
