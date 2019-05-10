// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/webui_graph_dump_impl.h"

#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"

namespace performance_manager {

WebUIGraphDumpImpl::WebUIGraphDumpImpl(GraphImpl* graph)
    : graph_(graph), binding_(this) {
  DCHECK(graph);
}

WebUIGraphDumpImpl::~WebUIGraphDumpImpl() {
  if (change_subscriber_) {
    graph_->UnregisterObserver(this);
    for (auto* node : graph_->nodes())
      node->RemoveObserver(this);
  }
}

void WebUIGraphDumpImpl::Bind(mojom::WebUIGraphDumpRequest request,
                              base::OnceClosure error_handler) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(std::move(error_handler));
}

namespace {

template <typename FunctionType>
void ForFrameAndOffspring(FrameNodeImpl* parent_frame, FunctionType on_frame) {
  on_frame(parent_frame);

  for (FrameNodeImpl* child_frame : parent_frame->child_frame_nodes())
    ForFrameAndOffspring(child_frame, on_frame);
}

}  // namespace

void WebUIGraphDumpImpl::SubscribeToChanges(
    mojom::WebUIGraphChangeStreamPtr change_subscriber) {
  change_subscriber_ = std::move(change_subscriber);

  // Send creation notifications for all existing nodes and subscribe to them.
  for (ProcessNodeImpl* process_node : graph_->GetAllProcessNodes()) {
    SendProcessNotification(process_node, true);
    process_node->AddObserver(this);
  }
  for (PageNodeImpl* page_node : graph_->GetAllPageNodes()) {
    SendPageNotification(page_node, true);
    page_node->AddObserver(this);

    // Dispatch preorder frame notifications.
    for (FrameNodeImpl* main_frame_node : page_node->main_frame_nodes()) {
      ForFrameAndOffspring(main_frame_node, [this](FrameNodeImpl* frame_node) {
        frame_node->AddObserver(this);
        this->SendFrameNotification(frame_node, true);
      });
    }
  }

  // Subscribe to future changes to the graph.
  graph_->RegisterObserver(this);
}

bool WebUIGraphDumpImpl::ShouldObserve(const NodeBase* node) {
  return true;
}

void WebUIGraphDumpImpl::OnNodeAdded(NodeBase* node) {
  switch (node->type()) {
    case FrameNodeImpl::Type():
      SendFrameNotification(FrameNodeImpl::FromNodeBase(node), true);
      break;
    case PageNodeImpl::Type():
      SendPageNotification(PageNodeImpl::FromNodeBase(node), true);
      break;
    case ProcessNodeImpl::Type():
      SendProcessNotification(ProcessNodeImpl::FromNodeBase(node), true);
      break;
    case SystemNodeImpl::Type():
      break;
    case NodeTypeEnum::kInvalidType:
      break;
  }
}

void WebUIGraphDumpImpl::OnBeforeNodeRemoved(NodeBase* node) {
  SendDeletionNotification(node);
}

void WebUIGraphDumpImpl::OnIsCurrentChanged(FrameNodeImpl* frame_node) {
  SendFrameNotification(frame_node, false);
}

void WebUIGraphDumpImpl::OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) {
  SendFrameNotification(frame_node, false);
}

void WebUIGraphDumpImpl::OnLifecycleStateChanged(FrameNodeImpl* frame_node) {
  SendFrameNotification(frame_node, false);
}

void WebUIGraphDumpImpl::OnIsVisibleChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnIsLoadingChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnUkmSourceIdChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnLifecycleStateChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnPageAlmostIdleChanged(PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnMainFrameNavigationCommitted(
    PageNodeImpl* page_node) {
  SendPageNotification(page_node, false);
}

void WebUIGraphDumpImpl::OnExpectedTaskQueueingDurationSample(
    ProcessNodeImpl* process_node) {
  SendProcessNotification(process_node, false);
}

void WebUIGraphDumpImpl::OnMainThreadTaskLoadIsLow(
    ProcessNodeImpl* process_node) {
  SendProcessNotification(process_node, false);
}

void WebUIGraphDumpImpl::OnRendererIsBloated(ProcessNodeImpl* process_node) {
  SendProcessNotification(process_node, false);
}

void WebUIGraphDumpImpl::SendFrameNotification(FrameNodeImpl* frame,
                                               bool created) {
  // TODO(https://crbug.com/961785): Add more frame properties.
  mojom::WebUIFrameInfoPtr frame_info = mojom::WebUIFrameInfo::New();

  frame_info->id = NodeBase::GetSerializationId(frame);

  auto* parent_frame = frame->parent_frame_node();
  frame_info->parent_frame_id = NodeBase::GetSerializationId(parent_frame);

  auto* process = frame->process_node();
  frame_info->process_id = NodeBase::GetSerializationId(process);

  auto* page = frame->page_node();
  frame_info->page_id = NodeBase::GetSerializationId(page);

  frame_info->url = frame->url();

  if (created)
    change_subscriber_->FrameCreated(std::move(frame_info));
  else
    change_subscriber_->FrameChanged(std::move(frame_info));
}

void WebUIGraphDumpImpl::SendPageNotification(PageNodeImpl* page,
                                              bool created) {
  // TODO(https://crbug.com/961785): Add more page properties.
  mojom::WebUIPageInfoPtr page_info = mojom::WebUIPageInfo::New();

  page_info->id = NodeBase::GetSerializationId(page);
  page_info->main_frame_url = page->main_frame_url();
  if (created)
    change_subscriber_->PageCreated(std::move(page_info));
  else
    change_subscriber_->PageChanged(std::move(page_info));
}

void WebUIGraphDumpImpl::SendProcessNotification(ProcessNodeImpl* process,
                                                 bool created) {
  // TODO(https://crbug.com/961785): Add more process properties.
  mojom::WebUIProcessInfoPtr process_info = mojom::WebUIProcessInfo::New();

  process_info->id = NodeBase::GetSerializationId(process);
  process_info->pid = process->process_id();
  process_info->cumulative_cpu_usage = process->cumulative_cpu_usage();
  process_info->private_footprint_kb = process->private_footprint_kb();

  if (created)
    change_subscriber_->ProcessCreated(std::move(process_info));
  else
    change_subscriber_->ProcessChanged(std::move(process_info));
}

void WebUIGraphDumpImpl::SendDeletionNotification(NodeBase* node) {
  change_subscriber_->NodeDeleted(NodeBase::GetSerializationId(node));
}

}  // namespace performance_manager
