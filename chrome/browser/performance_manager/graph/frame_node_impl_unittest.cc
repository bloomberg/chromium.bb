// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"

#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class FrameNodeImplTest : public GraphTestHarness {
 public:
  void SetUp() override {
    ResourceCoordinatorClock::SetClockForTesting(&clock_);

    // Sets a valid starting time.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void TearDown() override { ResourceCoordinatorClock::ResetClockForTesting(); }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock clock_;
};

using FrameNodeImplDeathTest = FrameNodeImplTest;

}  // namespace

TEST_F(FrameNodeImplTest, AddChildFrameBasic) {
  auto frame1_node = CreateNode<FrameNodeImpl>();
  auto frame2_node = CreateNode<FrameNodeImpl>();
  auto frame3_node = CreateNode<FrameNodeImpl>();

  frame1_node->AddChildFrame(frame2_node.get());
  frame1_node->AddChildFrame(frame3_node.get());
  EXPECT_EQ(nullptr, frame1_node->GetParentFrameNode());
  EXPECT_EQ(2u, frame1_node->child_frame_nodes_for_testing().size());
  EXPECT_EQ(frame1_node.get(), frame2_node->GetParentFrameNode());
  EXPECT_EQ(frame1_node.get(), frame3_node->GetParentFrameNode());
}

TEST_F(FrameNodeImplDeathTest, AddChildFrameOnCyclicReference) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  auto frame1_node = CreateNode<FrameNodeImpl>();
  auto frame2_node = CreateNode<FrameNodeImpl>();
  auto frame3_node = CreateNode<FrameNodeImpl>();

  frame1_node->AddChildFrame(frame2_node.get());
  frame2_node->AddChildFrame(frame3_node.get());
// |frame3_node| can't add |frame1_node| because |frame1_node| is an ancestor of
// |frame3_node|, and this will hit a DCHECK because of cyclic reference.
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  EXPECT_DEATH_IF_SUPPORTED(frame3_node->AddChildFrame(frame1_node.get()), "");
#else
  frame3_node->AddChildFrame(frame1_node.get());
#endif  // !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)

  EXPECT_EQ(1u, frame1_node->child_frame_nodes_for_testing().count(
                    frame2_node.get()));
  EXPECT_EQ(1u, frame2_node->child_frame_nodes_for_testing().count(
                    frame3_node.get()));
  // |frame1_node| was not added successfully because |frame1_node| is one of
  // the ancestors of |frame3_node|.
  EXPECT_EQ(0u, frame3_node->child_frame_nodes_for_testing().count(
                    frame1_node.get()));
}

TEST_F(FrameNodeImplTest, RemoveChildFrame) {
  auto parent_frame_node = CreateNode<FrameNodeImpl>();
  auto child_frame_node = CreateNode<FrameNodeImpl>();

  // Parent-child relationships have not been established yet.
  EXPECT_EQ(0u, parent_frame_node->child_frame_nodes_for_testing().size());
  EXPECT_TRUE(!parent_frame_node->GetParentFrameNode());
  EXPECT_EQ(0u, child_frame_node->child_frame_nodes_for_testing().size());
  EXPECT_TRUE(!child_frame_node->GetParentFrameNode());

  parent_frame_node->AddChildFrame(child_frame_node.get());

  // Ensure correct Parent-child relationships have been established.
  EXPECT_EQ(1u, parent_frame_node->child_frame_nodes_for_testing().size());
  EXPECT_TRUE(!parent_frame_node->GetParentFrameNode());
  EXPECT_EQ(0u, child_frame_node->child_frame_nodes_for_testing().size());
  EXPECT_EQ(parent_frame_node.get(), child_frame_node->GetParentFrameNode());

  parent_frame_node->RemoveChildFrame(child_frame_node.get());

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, parent_frame_node->child_frame_nodes_for_testing().size());
  EXPECT_TRUE(!parent_frame_node->GetParentFrameNode());
  EXPECT_EQ(0u, child_frame_node->child_frame_nodes_for_testing().size());
  EXPECT_TRUE(!child_frame_node->GetParentFrameNode());
}

int64_t GetLifecycleState(PageNodeImpl* cu) {
  int64_t value;
  if (cu->GetProperty(
          resource_coordinator::mojom::PropertyType::kLifecycleState, &value))
    return value;
  // Initial state is running.
  return static_cast<int64_t>(
      resource_coordinator::mojom::LifecycleState::kRunning);
}

#define EXPECT_FROZEN(cu)                                              \
  EXPECT_EQ(static_cast<int64_t>(                                      \
                resource_coordinator::mojom::LifecycleState::kFrozen), \
            GetLifecycleState(cu.get()))
#define EXPECT_RUNNING(cu)                                              \
  EXPECT_EQ(static_cast<int64_t>(                                       \
                resource_coordinator::mojom::LifecycleState::kRunning), \
            GetLifecycleState(cu.get()))

TEST_F(FrameNodeImplTest, LifecycleStatesTransitions) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  // Verifying the model.
  ASSERT_TRUE(mock_graph.frame->IsMainFrame());
  ASSERT_TRUE(mock_graph.other_frame->IsMainFrame());
  ASSERT_FALSE(mock_graph.child_frame->IsMainFrame());
  ASSERT_EQ(mock_graph.child_frame->GetParentFrameNode(),
            mock_graph.other_frame.get());
  ASSERT_EQ(mock_graph.frame->GetPageNode(), mock_graph.page.get());
  ASSERT_EQ(mock_graph.other_frame->GetPageNode(), mock_graph.other_page.get());

  // Freezing a child frame should not affect the page state.
  mock_graph.child_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  EXPECT_RUNNING(mock_graph.page);
  EXPECT_RUNNING(mock_graph.other_page);

  // Freezing the only frame in a page should freeze that page.
  mock_graph.frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  EXPECT_FROZEN(mock_graph.page);
  EXPECT_RUNNING(mock_graph.other_page);

  // Unfreeze the child frame in the other page.
  mock_graph.child_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kRunning);
  EXPECT_FROZEN(mock_graph.page);
  EXPECT_RUNNING(mock_graph.other_page);

  // Freezing the main frame in the other page should not alter that pages
  // state, as there is still a child frame that is running.
  mock_graph.other_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  EXPECT_FROZEN(mock_graph.page);
  EXPECT_RUNNING(mock_graph.other_page);

  // Refreezing the child frame should freeze the page.
  mock_graph.child_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  EXPECT_FROZEN(mock_graph.page);
  EXPECT_FROZEN(mock_graph.other_page);

  // Unfreezing a main frame should unfreeze the associated page.
  mock_graph.frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kRunning);
  EXPECT_RUNNING(mock_graph.page);
  EXPECT_FROZEN(mock_graph.other_page);

  // Unfreezing the child frame should unfreeze the associated page.
  mock_graph.child_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kRunning);
  EXPECT_RUNNING(mock_graph.page);
  EXPECT_RUNNING(mock_graph.other_page);

  // Unfreezing the main frame shouldn't change anything.
  mock_graph.other_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kRunning);
  EXPECT_RUNNING(mock_graph.page);
  EXPECT_RUNNING(mock_graph.other_page);
}

}  // namespace performance_manager
