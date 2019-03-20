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
      frame(TestNodeWrapper<FrameNodeImpl>::Create(graph)),
      process(TestNodeWrapper<ProcessNodeImpl>::Create(graph)),
      page(TestNodeWrapper<PageNodeImpl>::Create(graph)) {
  frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  page->AddFrame(frame->id());
  frame->SetProcess(process->id());
  process->SetPID(1);
}

MockSinglePageInSingleProcessGraph::~MockSinglePageInSingleProcessGraph() =
    default;

MockMultiplePagesInSingleProcessGraph::MockMultiplePagesInSingleProcessGraph(
    Graph* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      other_frame(TestNodeWrapper<FrameNodeImpl>::Create(graph)),
      other_page(TestNodeWrapper<PageNodeImpl>::Create(graph)) {
  other_frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  other_page->AddFrame(other_frame->id());
  other_frame->SetProcess(process->id());
}

MockMultiplePagesInSingleProcessGraph::
    ~MockMultiplePagesInSingleProcessGraph() = default;

MockSinglePageWithMultipleProcessesGraph::
    MockSinglePageWithMultipleProcessesGraph(Graph* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      child_frame(TestNodeWrapper<FrameNodeImpl>::Create(graph)),
      other_process(TestNodeWrapper<ProcessNodeImpl>::Create(graph)) {
  child_frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  frame->AddChildFrame(child_frame->id());
  page->AddFrame(child_frame->id());
  child_frame->SetProcess(other_process->id());
  other_process->SetPID(2);
}

MockSinglePageWithMultipleProcessesGraph::
    ~MockSinglePageWithMultipleProcessesGraph() = default;

MockMultiplePagesWithMultipleProcessesGraph::
    MockMultiplePagesWithMultipleProcessesGraph(Graph* graph)
    : MockMultiplePagesInSingleProcessGraph(graph),
      child_frame(TestNodeWrapper<FrameNodeImpl>::Create(graph)),
      other_process(TestNodeWrapper<ProcessNodeImpl>::Create(graph)) {
  child_frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  other_frame->AddChildFrame(child_frame->id());
  other_page->AddFrame(child_frame->id());
  child_frame->SetProcess(other_process->id());
  other_process->SetPID(2);
}

MockMultiplePagesWithMultipleProcessesGraph::
    ~MockMultiplePagesWithMultipleProcessesGraph() = default;

}  // namespace performance_manager
