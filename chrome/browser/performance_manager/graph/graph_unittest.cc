// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph.h"

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace {

ProcessNodeImpl* CreateProcessNode(Graph* graph) {
  return graph->CreateProcessNode(
      resource_coordinator::CoordinationUnitID(
          resource_coordinator::CoordinationUnitType::kProcess,
          resource_coordinator::CoordinationUnitID::RANDOM_ID),
      nullptr);
}

}  // namespace

TEST(GraphTest, DestructionWhileCUSOutstanding) {
  std::unique_ptr<Graph> graph(new Graph());

  for (size_t i = 0; i < 10; ++i) {
    ProcessNodeImpl* process = CreateProcessNode(graph.get());
    EXPECT_NE(nullptr, process);

    process->SetPID(i + 100);
  }

  EXPECT_NE(nullptr, graph->FindOrCreateSystemNode(nullptr));

  // This should destroy all the CUs without incident.
  graph.reset();
}

TEST(GraphTest, FindOrCreateSystemNode) {
  Graph graph;

  SystemNodeImpl* system_cu = graph.FindOrCreateSystemNode(nullptr);

  // A second request should return the same instance.
  EXPECT_EQ(system_cu, graph.FindOrCreateSystemNode(nullptr));

  // Destructing the system CU should be allowed.
  system_cu->Destruct();

  system_cu = graph.FindOrCreateSystemNode(nullptr);
  EXPECT_NE(nullptr, system_cu);
}

TEST(GraphTest, GetProcessNodeByPid) {
  Graph graph;

  ProcessNodeImpl* process = CreateProcessNode(&graph);
  EXPECT_EQ(base::kNullProcessId, process->process_id());

  static constexpr base::ProcessId kPid = 10;

  EXPECT_EQ(nullptr, graph.GetProcessNodeByPid(kPid));
  process->SetPID(kPid);
  EXPECT_EQ(process, graph.GetProcessNodeByPid(kPid));

  process->Destruct();

  EXPECT_EQ(nullptr, graph.GetProcessNodeByPid(12));
}

TEST(GraphTest, PIDReuse) {
  // This test emulates what happens on Windows under aggressive PID reuse,
  // where a process termination notification can be delayed until after the
  // PID has been reused for a new process.
  Graph graph;

  static constexpr base::ProcessId kPid = 10;

  ProcessNodeImpl* process1 = CreateProcessNode(&graph);
  ProcessNodeImpl* process2 = CreateProcessNode(&graph);

  process1->SetPID(kPid);
  EXPECT_EQ(process1, graph.GetProcessNodeByPid(kPid));

  // The second registration for the same PID should override the first one.
  process2->SetPID(kPid);
  EXPECT_EQ(process2, graph.GetProcessNodeByPid(kPid));

  // The destruction of the first process CU shouldn't clear the PID
  // registration.
  process1->Destruct();
  EXPECT_EQ(process2, graph.GetProcessNodeByPid(kPid));
}

TEST(GraphTest, GetAllCUsByType) {
  Graph graph;
  MockMultiplePagesInSingleProcessGraph cu_graph(&graph);

  std::vector<ProcessNodeImpl*> processes = graph.GetAllProcessNodes();
  ASSERT_EQ(1u, processes.size());
  EXPECT_NE(nullptr, processes[0]);

  std::vector<FrameNodeImpl*> frames = graph.GetAllFrameNodes();
  ASSERT_EQ(2u, frames.size());
  EXPECT_NE(nullptr, frames[0]);
  EXPECT_NE(nullptr, frames[1]);

  std::vector<PageNodeImpl*> pages = graph.GetAllPageNodes();
  ASSERT_EQ(2u, pages.size());
  EXPECT_NE(nullptr, pages[0]);
  EXPECT_NE(nullptr, pages[1]);
}

}  // namespace performance_manager
