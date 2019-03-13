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

  return main_frame->GetProcessNode() == process;
}

}  // namespace

namespace performance_manager {

CoordinationUnitIntrospectorImpl::CoordinationUnitIntrospectorImpl(Graph* graph)
    : graph_(graph) {}

CoordinationUnitIntrospectorImpl::~CoordinationUnitIntrospectorImpl() = default;

void CoordinationUnitIntrospectorImpl::GetProcessToURLMap(
    GetProcessToURLMapCallback callback) {
  std::vector<resource_coordinator::mojom::ProcessInfoPtr> process_infos;
  std::vector<ProcessNodeImpl*> process_cus = graph_->GetAllProcessNodes();
  for (auto* process_cu : process_cus) {
    int64_t pid;
    if (!process_cu->GetProperty(
            resource_coordinator::mojom::PropertyType::kPID, &pid))
      continue;

    resource_coordinator::mojom::ProcessInfoPtr process_info(
        resource_coordinator::mojom::ProcessInfo::New());
    process_info->pid = base::checked_cast<base::ProcessId>(pid);
    DCHECK_NE(base::kNullProcessId, process_info->pid);
    process_info->launch_time = process_cu->launch_time();

    std::set<PageNodeImpl*> page_cus =
        process_cu->GetAssociatedPageCoordinationUnits();
    for (PageNodeImpl* page_cu : page_cus) {
      int64_t ukm_source_id;
      if (page_cu->GetProperty(
              resource_coordinator::mojom::PropertyType::kUKMSourceId,
              &ukm_source_id)) {
        resource_coordinator::mojom::PageInfoPtr page_info(
            resource_coordinator::mojom::PageInfo::New());
        page_info->ukm_source_id = ukm_source_id;
        page_info->tab_id = page_cu->id().id;
        page_info->hosts_main_frame = HostsMainFrame(process_cu, page_cu);
        page_info->is_visible = page_cu->is_visible();
        page_info->time_since_last_visibility_change =
            page_cu->TimeSinceLastVisibilityChange();
        page_info->time_since_last_navigation =
            page_cu->TimeSinceLastNavigation();
        process_info->page_infos.push_back(std::move(page_info));
      }
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
