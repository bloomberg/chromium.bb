// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/webui_graph_dump_impl.h"

#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

WebUIGraphDumpImpl::WebUIGraphDumpImpl(GraphImpl* graph)
    : graph_(graph), binding_(this) {
  DCHECK(graph);
}

WebUIGraphDumpImpl::~WebUIGraphDumpImpl() {}

void WebUIGraphDumpImpl::Bind(mojom::WebUIGraphDumpRequest request,
                              base::OnceClosure error_handler) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(std::move(error_handler));
}

void WebUIGraphDumpImpl::GetCurrentGraph(GetCurrentGraphCallback callback) {
  mojom::WebUIGraphPtr graph = mojom::WebUIGraph::New();

  {
    auto processes = graph_->GetAllProcessNodes();
    graph->processes.reserve(processes.size());
    for (auto* process : processes) {
      mojom::WebUIProcessInfoPtr process_info = mojom::WebUIProcessInfo::New();

      process_info->id = NodeBase::GetSerializationId(process);
      process_info->pid = process->process_id();
      process_info->cumulative_cpu_usage = process->cumulative_cpu_usage();
      process_info->private_footprint_kb = process->private_footprint_kb();

      graph->processes.push_back(std::move(process_info));
    }
  }

  {
    auto frames = graph_->GetAllFrameNodes();
    graph->frames.reserve(frames.size());
    for (auto* frame : frames) {
      mojom::WebUIFrameInfoPtr frame_info = mojom::WebUIFrameInfo::New();

      frame_info->id = NodeBase::GetSerializationId(frame);

      auto* parent_frame = frame->parent_frame_node();
      frame_info->parent_frame_id = NodeBase::GetSerializationId(parent_frame);

      auto* process = frame->process_node();
      frame_info->process_id = NodeBase::GetSerializationId(process);

      frame_info->url = frame->url();

      graph->frames.push_back(std::move(frame_info));
    }
  }

  {
    auto pages = graph_->GetAllPageNodes();
    graph->pages.reserve(pages.size());
    for (auto* page : pages) {
      mojom::WebUIPageInfoPtr page_info = mojom::WebUIPageInfo::New();

      page_info->id = NodeBase::GetSerializationId(page);
      page_info->main_frame_url = page->main_frame_url();

      auto* main_frame = page->GetMainFrameNode();
      page_info->main_frame_id = NodeBase::GetSerializationId(main_frame);

      graph->pages.push_back(std::move(page_info));
    }
  }
  std::move(callback).Run(std::move(graph));
}

}  // namespace performance_manager
