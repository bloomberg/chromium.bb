// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph_introspector_impl.h"

#include <set>
#include <utility>
#include <vector>

#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace {

using performance_manager::FrameNodeImpl;
using performance_manager::PageNodeImpl;
using performance_manager::ProcessNodeImpl;

// Returns true iff the given |process| is responsible for hosting the
// main-frame of the given |page|.
bool HostsMainFrame(ProcessNodeImpl* process, PageNodeImpl* page) {
  FrameNodeImpl* main_frame = page->GetMainFrameNode();
  if (main_frame == nullptr) {
    // |process| can't host a frame that doesn't exist.
    return false;
  }

  return main_frame->process_node() == process;
}

}  // namespace

namespace performance_manager {

CoordinationUnitIntrospectorImpl::CoordinationUnitIntrospectorImpl(Graph* graph)
    : graph_(graph) {}

CoordinationUnitIntrospectorImpl::~CoordinationUnitIntrospectorImpl() = default;

void CoordinationUnitIntrospectorImpl::GetProcessToURLMap(
    GetProcessToURLMapCallback callback) {
  std::vector<resource_coordinator::mojom::ProcessInfoPtr> process_infos;
  std::vector<ProcessNodeImpl*> process_nodes = graph_->GetAllProcessNodes();
  for (auto* process_node : process_nodes) {
    if (process_node->process_id() == base::kNullProcessId)
      continue;

    resource_coordinator::mojom::ProcessInfoPtr process_info(
        resource_coordinator::mojom::ProcessInfo::New());
    process_info->pid = process_node->process_id();
    process_info->launch_time = process_node->launch_time();

    std::set<PageNodeImpl*> page_nodes =
        process_node->GetAssociatedPageCoordinationUnits();
    for (PageNodeImpl* page_node : page_nodes) {
      if (page_node->ukm_source_id() == ukm::kInvalidSourceId)
        continue;

      resource_coordinator::mojom::PageInfoPtr page_info(
          resource_coordinator::mojom::PageInfo::New());
      page_info->ukm_source_id = page_node->ukm_source_id();
      page_info->tab_id = page_node->id().id;
      page_info->hosts_main_frame = HostsMainFrame(process_node, page_node);
      page_info->is_visible = page_node->is_visible();
      page_info->time_since_last_visibility_change =
          page_node->TimeSinceLastVisibilityChange();
      page_info->time_since_last_navigation =
          page_node->TimeSinceLastNavigation();
      process_info->page_infos.push_back(std::move(page_info));
    }
    process_infos.push_back(std::move(process_info));
  }
  std::move(callback).Run(std::move(process_infos));
}

void CoordinationUnitIntrospectorImpl::BindToInterface(
    resource_coordinator::mojom::CoordinationUnitIntrospectorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace performance_manager
