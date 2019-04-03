// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/process_node_impl.h"

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class ProcessNodeImplTest : public GraphTestHarness {};

class MockGraphObserver : public GraphObserver {
 public:
  MockGraphObserver() = default;
  virtual ~MockGraphObserver() = default;

  bool ShouldObserve(const NodeBase* node) override {
    return node->id().type ==
           resource_coordinator::CoordinationUnitType::kProcess;
  }

  MOCK_METHOD1(OnAllFramesInProcessFrozen, void(ProcessNodeImpl*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGraphObserver);
};

}  // namespace

TEST_F(ProcessNodeImplTest, MeasureCPUUsage) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->SetCPUUsage(1.0);
  EXPECT_EQ(1.0, process_node->cpu_usage());
}

TEST_F(ProcessNodeImplTest, OnAllFramesInProcessFrozen) {
  testing::StrictMock<MockGraphObserver> observer;
  graph()->RegisterObserver(&observer);
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  // 1/2 frame in the process is frozen.
  // No call to OnAllFramesInProcessFrozen() is expected.
  mock_graph.frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);

  // 2/2 frames in the process are frozen.
  EXPECT_CALL(observer, OnAllFramesInProcessFrozen(mock_graph.process.get()));
  mock_graph.other_frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  testing::Mock::VerifyAndClear(&observer);

  // A frame is unfrozen and frozen.
  mock_graph.frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kRunning);
  EXPECT_CALL(observer, OnAllFramesInProcessFrozen(mock_graph.process.get()));
  mock_graph.frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  testing::Mock::VerifyAndClear(&observer);

  // A frozen frame is frozen again.
  // No call to OnAllFramesInProcessFrozen() is expected.
  mock_graph.frame->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);

  graph()->UnregisterObserver(&observer);
}

TEST_F(ProcessNodeImplTest, ProcessLifeCycle) {
  auto process_node = CreateNode<ProcessNodeImpl>();

  // Test the potential lifecycles of a process CU.
  // First go to exited without an intervening PID, as would happen
  // in the case the process fails to start.
  EXPECT_FALSE(process_node->exit_status());
  constexpr int32_t kExitStatus = 0xF00;
  process_node->SetProcessExitStatus(kExitStatus);
  EXPECT_TRUE(process_node->exit_status());
  EXPECT_EQ(kExitStatus, process_node->exit_status().value());

  // Next go through PID->exit status.
  constexpr base::ProcessId kTestPid = 0xCAFE;
  process_node->SetPID(kTestPid);
  // Resurrection should clear the exit status.
  EXPECT_FALSE(process_node->exit_status());

  EXPECT_EQ(base::Time(), process_node->launch_time());
  EXPECT_EQ(0U, process_node->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta(), process_node->cumulative_cpu_usage());

  process_node->SetLaunchTime(base::Time::Now());
  process_node->set_private_footprint_kb(10U);
  process_node->set_cumulative_cpu_usage(base::TimeDelta::FromMicroseconds(1));

  // Kill it again and verify the properties above stick around.
  process_node->SetProcessExitStatus(kExitStatus);

  EXPECT_NE(base::Time(), process_node->launch_time());
  EXPECT_NE(0U, process_node->private_footprint_kb());
  EXPECT_NE(base::TimeDelta(), process_node->cumulative_cpu_usage());

  // Resurrect again and verify the launch time and measurements
  // are cleared.
  process_node->SetPID(kTestPid);
  EXPECT_EQ(base::Time(), process_node->launch_time());
  EXPECT_EQ(0U, process_node->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta(), process_node->cumulative_cpu_usage());
}

}  // namespace performance_manager
