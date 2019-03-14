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
  // Create a node of each type.
  std::unique_ptr<FrameNodeImpl> frame_node =
      performance_manager()->CreateFrameNode();
  EXPECT_NE(nullptr, frame_node.get());

  std::unique_ptr<PageNodeImpl> page_node =
      performance_manager()->CreatePageNode();
  EXPECT_NE(nullptr, page_node.get());

  std::unique_ptr<ProcessNodeImpl> process_node =
      performance_manager()->CreateProcessNode();
  EXPECT_NE(nullptr, process_node.get());

  performance_manager()->DeleteNode(std::move(process_node));
  performance_manager()->DeleteNode(std::move(page_node));
  performance_manager()->DeleteNode(std::move(frame_node));
}

// TODO(siggi): More tests!
// - Test the WebUI interface.
// - Test the graph introspector interface.

}  // namespace performance_manager
