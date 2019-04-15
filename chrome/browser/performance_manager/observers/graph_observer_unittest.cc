// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/graph_observer.h"

#include <memory>

#include "base/process/process_handle.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class GraphObserverTest : public GraphTestHarness {};

class TestGraphObserver : public GraphObserver {
 public:
  TestGraphObserver() : node_created_count_(0u), node_destroyed_count_(0u) {}

  size_t node_created_count() { return node_created_count_; }
  size_t node_destroyed_count() { return node_destroyed_count_; }

  // Overridden from GraphObserver.
  bool ShouldObserve(const NodeBase* node) override {
    return node->id().type ==
           resource_coordinator::CoordinationUnitType::kFrame;
  }
  void OnNodeAdded(NodeBase* node) override { ++node_created_count_; }
  void OnBeforeNodeRemoved(NodeBase* node) override { ++node_destroyed_count_; }

 private:
  size_t node_created_count_;
  size_t node_destroyed_count_;
};

}  // namespace

TEST_F(GraphObserverTest, CallbacksInvoked) {
  EXPECT_TRUE(graph()->observers_for_testing().empty());
  auto observer = std::make_unique<TestGraphObserver>();
  graph()->RegisterObserver(observer.get());
  EXPECT_EQ(1u, graph()->observers_for_testing().size());


  {
    auto process_node = CreateNode<ProcessNodeImpl>();
    auto page_node = CreateNode<PageNodeImpl>(nullptr);
    auto root_frame_node = CreateNode<FrameNodeImpl>(
        process_node.get(), page_node.get(), nullptr, 0);
    auto frame_node = CreateNode<FrameNodeImpl>(
        process_node.get(), page_node.get(), root_frame_node.get(), 1);

    EXPECT_EQ(2u, observer->node_created_count());
  }

  EXPECT_EQ(2u, observer->node_destroyed_count());

  graph()->UnregisterObserver(observer.get());
}

}  // namespace performance_manager
