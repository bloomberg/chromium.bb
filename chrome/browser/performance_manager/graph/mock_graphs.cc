// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/mock_graphs.h"

#include <string>

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"

namespace performance_manager {

MockSinglePageInSingleProcessGraph::MockSinglePageInSingleProcessGraph(
    Graph* graph)
    : system(TestNodeWrapper<SystemNodeImpl>::Create(graph)),
      process(TestNodeWrapper<ProcessNodeImpl>::Create(graph)),
      page(TestNodeWrapper<PageNodeImpl>::Create(graph)),
      frame(TestNodeWrapper<FrameNodeImpl>::Create(graph,
                                                   process.get(),
                                                   page.get(),
                                                   nullptr,
                                                   0)) {
  frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  process->SetPID(1);
}

MockSinglePageInSingleProcessGraph::~MockSinglePageInSingleProcessGraph() {
  // Make sure frame nodes are torn down before pages.
  frame.reset();
  page.reset();
}

MockMultiplePagesInSingleProcessGraph::MockMultiplePagesInSingleProcessGraph(
    Graph* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      other_page(TestNodeWrapper<PageNodeImpl>::Create(graph)),
      other_frame(TestNodeWrapper<FrameNodeImpl>::Create(graph,
                                                         process.get(),
                                                         other_page.get(),
                                                         nullptr,
                                                         1)) {
  other_frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
}

MockMultiplePagesInSingleProcessGraph::
    ~MockMultiplePagesInSingleProcessGraph() {
  other_frame.reset();
  other_page.reset();
}

MockSinglePageWithMultipleProcessesGraph::
    MockSinglePageWithMultipleProcessesGraph(Graph* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      other_process(TestNodeWrapper<ProcessNodeImpl>::Create(graph)),
      child_frame(TestNodeWrapper<FrameNodeImpl>::Create(graph,
                                                         other_process.get(),
                                                         page.get(),
                                                         frame.get(),
                                                         2)) {
  other_process->SetPID(2);
  child_frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
}

MockSinglePageWithMultipleProcessesGraph::
    ~MockSinglePageWithMultipleProcessesGraph() = default;

MockMultiplePagesWithMultipleProcessesGraph::
    MockMultiplePagesWithMultipleProcessesGraph(Graph* graph)
    : MockMultiplePagesInSingleProcessGraph(graph),
      other_process(TestNodeWrapper<ProcessNodeImpl>::Create(graph)),
      child_frame(TestNodeWrapper<FrameNodeImpl>::Create(graph,
                                                         other_process.get(),
                                                         other_page.get(),
                                                         other_frame.get(),
                                                         3)) {
  other_process->SetPID(2);
  child_frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
}

MockMultiplePagesWithMultipleProcessesGraph::
    ~MockMultiplePagesWithMultipleProcessesGraph() = default;

}  // namespace performance_manager
