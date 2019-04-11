// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"

#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class FrameNodeImplTest : public GraphTestHarness {
 public:
  void SetUp() override {
    PerformanceManagerClock::SetClockForTesting(&clock_);

    // Sets a valid starting time.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void TearDown() override { PerformanceManagerClock::ResetClockForTesting(); }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock clock_;
};

}  // namespace

TEST_F(FrameNodeImplTest, AddFrameHierarchyBasic) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>(nullptr /*TEST*/);
  auto parent_node =
      CreateNode<FrameNodeImpl>(process.get(), page.get(), nullptr, 0);
  auto child2_node = CreateNode<FrameNodeImpl>(process.get(), page.get(),
                                               parent_node.get(), 1);
  auto child3_node = CreateNode<FrameNodeImpl>(process.get(), page.get(),
                                               parent_node.get(), 2);

  EXPECT_EQ(nullptr, parent_node->parent_frame_node());
  EXPECT_EQ(2u, parent_node->child_frame_nodes().size());
  EXPECT_EQ(parent_node.get(), child2_node->parent_frame_node());
  EXPECT_EQ(parent_node.get(), child3_node->parent_frame_node());
}

TEST_F(FrameNodeImplTest, Url) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>(nullptr /*TEST*/);
  auto frame_node =
      CreateNode<FrameNodeImpl>(process.get(), page.get(), nullptr, 0);
  EXPECT_TRUE(frame_node->url().is_empty());
  const GURL url("http://www.foo.com/");
  frame_node->set_url(url);
  EXPECT_EQ(url, frame_node->url());
}

TEST_F(FrameNodeImplTest, RemoveChildFrame) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>(nullptr /*TEST*/);
  auto parent_frame_node =
      CreateNode<FrameNodeImpl>(process.get(), page.get(), nullptr, 0);
  auto child_frame_node = CreateNode<FrameNodeImpl>(process.get(), page.get(),
                                                    parent_frame_node.get(), 1);

  // Ensure correct Parent-child relationships have been established.
  EXPECT_EQ(1u, parent_frame_node->child_frame_nodes().size());
  EXPECT_TRUE(!parent_frame_node->parent_frame_node());
  EXPECT_EQ(0u, child_frame_node->child_frame_nodes().size());
  EXPECT_EQ(parent_frame_node.get(), child_frame_node->parent_frame_node());

  child_frame_node.reset();

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, parent_frame_node->child_frame_nodes().size());
  EXPECT_TRUE(!parent_frame_node->parent_frame_node());
}

TEST_F(FrameNodeImplTest, LifecycleStatesTransitions) {
  const auto kRunning = resource_coordinator::mojom::LifecycleState::kRunning;
  const auto kFrozen = resource_coordinator::mojom::LifecycleState::kFrozen;
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());

  // Verifying the model.
  ASSERT_TRUE(mock_graph.frame->IsMainFrame());
  ASSERT_TRUE(mock_graph.other_frame->IsMainFrame());
  ASSERT_FALSE(mock_graph.child_frame->IsMainFrame());
  ASSERT_EQ(mock_graph.child_frame->parent_frame_node(),
            mock_graph.other_frame.get());
  ASSERT_EQ(mock_graph.frame->page_node(), mock_graph.page.get());
  ASSERT_EQ(mock_graph.other_frame->page_node(), mock_graph.other_page.get());

  // Freezing a child frame should not affect the page state.
  mock_graph.child_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  EXPECT_EQ(kRunning, mock_graph.page->lifecycle_state());
  EXPECT_EQ(kRunning, mock_graph.other_page->lifecycle_state());

  // Freezing the only frame in a page should freeze that page.
  mock_graph.frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  EXPECT_EQ(kFrozen, mock_graph.page->lifecycle_state());
  EXPECT_EQ(kRunning, mock_graph.other_page->lifecycle_state());

  // Unfreeze the child frame in the other page.
  mock_graph.child_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kRunning);
  EXPECT_EQ(kFrozen, mock_graph.page->lifecycle_state());
  EXPECT_EQ(kRunning, mock_graph.other_page->lifecycle_state());

  // Freezing the main frame in the other page should not alter that pages
  // state, as there is still a child frame that is running.
  mock_graph.other_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  EXPECT_EQ(kFrozen, mock_graph.page->lifecycle_state());
  EXPECT_EQ(kRunning, mock_graph.other_page->lifecycle_state());

  // Refreezing the child frame should freeze the page.
  mock_graph.child_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  EXPECT_EQ(kFrozen, mock_graph.page->lifecycle_state());
  EXPECT_EQ(kFrozen, mock_graph.other_page->lifecycle_state());

  // Unfreezing a main frame should unfreeze the associated page.
  mock_graph.frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kRunning);
  EXPECT_EQ(kRunning, mock_graph.page->lifecycle_state());
  EXPECT_EQ(kFrozen, mock_graph.other_page->lifecycle_state());

  // Unfreezing the child frame should unfreeze the associated page.
  mock_graph.child_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kRunning);
  EXPECT_EQ(kRunning, mock_graph.page->lifecycle_state());
  EXPECT_EQ(kRunning, mock_graph.other_page->lifecycle_state());

  // Unfreezing the main frame shouldn't change anything.
  mock_graph.other_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kRunning);
  EXPECT_EQ(kRunning, mock_graph.page->lifecycle_state());
  EXPECT_EQ(kRunning, mock_graph.other_page->lifecycle_state());
}

}  // namespace performance_manager
