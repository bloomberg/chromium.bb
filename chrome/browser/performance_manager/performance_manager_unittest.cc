// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager.h"

#include <utility>

#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class PerformanceManagerTest : public testing::Test {
 public:
  PerformanceManagerTest() {}

  ~PerformanceManagerTest() override {}

  void SetUp() override {
    EXPECT_EQ(nullptr, PerformanceManager::GetInstance());
    performance_manager_ = PerformanceManager::Create();
    // Make sure creation registers the created instance.
    EXPECT_EQ(performance_manager_.get(), PerformanceManager::GetInstance());
  }

  void TearDown() override {
    if (performance_manager_) {
      PerformanceManager::Destroy(std::move(performance_manager_));
      // Make sure destruction unregisters the instance.
      EXPECT_EQ(nullptr, PerformanceManager::GetInstance());
    }

    task_environment_.RunUntilIdle();
  }

 protected:
  PerformanceManager* performance_manager() {
    return performance_manager_.get();
  }

 private:
  std::unique_ptr<PerformanceManager> performance_manager_;
  base::test::ScopedTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerTest);
};

TEST_F(PerformanceManagerTest, InstantiateNodes) {
  std::unique_ptr<PageNodeImpl> page_node =
      performance_manager()->CreatePageNode();
  EXPECT_NE(nullptr, page_node.get());

  // Create a node of each type.
  std::unique_ptr<FrameNodeImpl> frame_node =
      performance_manager()->CreateFrameNode(page_node.get(), nullptr);
  EXPECT_NE(nullptr, frame_node.get());

  std::unique_ptr<ProcessNodeImpl> process_node =
      performance_manager()->CreateProcessNode();
  EXPECT_NE(nullptr, process_node.get());

  performance_manager()->DeleteNode(std::move(process_node));
  performance_manager()->DeleteNode(std::move(frame_node));
  performance_manager()->DeleteNode(std::move(page_node));
}

TEST_F(PerformanceManagerTest, BatchDeleteNodes) {
  // Create a page node and a small hierarchy of frames.
  std::unique_ptr<PageNodeImpl> page_node =
      performance_manager()->CreatePageNode();

  std::unique_ptr<FrameNodeImpl> parent1_frame =
      performance_manager()->CreateFrameNode(page_node.get(), nullptr);
  std::unique_ptr<FrameNodeImpl> parent2_frame =
      performance_manager()->CreateFrameNode(page_node.get(), nullptr);

  std::unique_ptr<FrameNodeImpl> child1_frame =
      performance_manager()->CreateFrameNode(page_node.get(),
                                             parent1_frame.get());
  std::unique_ptr<FrameNodeImpl> child2_frame =
      performance_manager()->CreateFrameNode(page_node.get(),
                                             parent2_frame.get());

  std::vector<std::unique_ptr<NodeBase>> nodes;
  for (size_t i = 0; i < 10; ++i) {
    nodes.push_back(performance_manager()->CreateFrameNode(page_node.get(),
                                                           child1_frame.get()));
    nodes.push_back(performance_manager()->CreateFrameNode(page_node.get(),
                                                           child1_frame.get()));
  }

  nodes.push_back(std::move(page_node));
  nodes.push_back(std::move(parent1_frame));
  nodes.push_back(std::move(parent2_frame));
  nodes.push_back(std::move(child1_frame));
  nodes.push_back(std::move(child2_frame));

  performance_manager()->BatchDeleteNodes(std::move(nodes));
}

// TODO(siggi): More tests!
// - Test the WebUI interface.
// - Test the graph introspector interface.

}  // namespace performance_manager
