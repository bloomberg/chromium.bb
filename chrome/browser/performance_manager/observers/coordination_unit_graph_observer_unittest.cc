// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"

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
  TestGraphObserver()
      : node_created_count_(0u),
        node_destroyed_count_(0u),
        property_changed_count_(0u) {}

  size_t node_created_count() { return node_created_count_; }
  size_t node_destroyed_count() { return node_destroyed_count_; }
  size_t property_changed_count() { return property_changed_count_; }

  // Overridden from GraphObserver.
  bool ShouldObserve(const NodeBase* node) override {
    return node->id().type ==
           resource_coordinator::CoordinationUnitType::kFrame;
  }
  void OnNodeAdded(NodeBase* node) override { ++node_created_count_; }
  void OnBeforeNodeRemoved(NodeBase* node) override { ++node_destroyed_count_; }
  void OnFramePropertyChanged(
      FrameNodeImpl* frame_node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override {
    ++property_changed_count_;
  }

 private:
  size_t node_created_count_;
  size_t node_destroyed_count_;
  size_t property_changed_count_;
};

}  // namespace

TEST_F(GraphObserverTest, CallbacksInvoked) {
  EXPECT_TRUE(graph()->observers_for_testing().empty());
  graph()->RegisterObserver(std::make_unique<TestGraphObserver>());
  EXPECT_EQ(1u, graph()->observers_for_testing().size());

  TestGraphObserver* observer = static_cast<TestGraphObserver*>(
      graph()->observers_for_testing()[0].get());

  {
    auto process_node = CreateNode<ProcessNodeImpl>();
    auto root_frame_node = CreateNode<FrameNodeImpl>();
    auto frame_node = CreateNode<FrameNodeImpl>();

    EXPECT_EQ(2u, observer->node_created_count());

    // The registered observer will only observe the events that happen to
    // |root_frame_node| and |frame_node| because
    // they are resource_coordinator::CoordinationUnitType::kFrame, so
    // OnPropertyChanged will only be called for |root_frame_node|.
    root_frame_node->SetPropertyForTesting(42);
    process_node->SetPropertyForTesting(42);
    EXPECT_EQ(1u, observer->property_changed_count());
  }

  EXPECT_EQ(2u, observer->node_destroyed_count());
}

}  // namespace performance_manager
