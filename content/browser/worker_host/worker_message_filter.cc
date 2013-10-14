// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_message_filter.h"

#include "content/browser/message_port_message_filter.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/resource_context.h"

namespace content {

WorkerMessageFilter::WorkerMessageFilter(
    int render_process_id,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition,
    MessagePortMessageFilter* message_port_message_filter)
    : render_process_id_(render_process_id),
      resource_context_(resource_context),
      partition_(partition),
      message_port_message_filter_(message_port_message_filter) {
  // Note: This constructor is called on both IO or UI thread.
  DCHECK(resource_context);
}

WorkerMessageFilter::~WorkerMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void WorkerMessageFilter::OnChannelClosing() {
  WorkerServiceImpl::GetInstance()->OnWorkerMessageFilterClosing(this);
}

bool WorkerMessageFilter::OnMessageReceived(const IPC::Message& message,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WorkerMessageFilter, message, *message_was_ok)
    // Worker messages.
    // Only sent from renderer for now, until we have nested workers.
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWorker, OnCreateWorker)
    // Only sent from renderer for now, until we have nested workers.
    IPC_MESSAGE_HANDLER(ViewHostMsg_LookupSharedWorker, OnLookupSharedWorker)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToWorker, OnForwardToWorker)
    // Only sent from renderer.
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentDetached, OnDocumentDetached)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

int WorkerMessageFilter::GetNextRoutingID() {
  return message_port_message_filter_->GetNextRoutingID();
}

void WorkerMessageFilter::OnCreateWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    int* route_id) {
  *route_id = params.route_id != MSG_ROUTING_NONE ?
      params.route_id : GetNextRoutingID();
  WorkerServiceImpl::GetInstance()->CreateWorker(
      params, *route_id, this, resource_context_, partition_);
}

void WorkerMessageFilter::OnLookupSharedWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    bool* exists,
    int* route_id,
    bool* url_error) {
  *route_id = GetNextRoutingID();

  WorkerServiceImpl::GetInstance()->LookupSharedWorker(
      params, *route_id, this, resource_context_, partition_, exists,
      url_error);
}

void WorkerMessageFilter::OnForwardToWorker(const IPC::Message& message) {
  WorkerServiceImpl::GetInstance()->ForwardToWorker(message, this);
}

void WorkerMessageFilter::OnDocumentDetached(unsigned long long document_id) {
  WorkerServiceImpl::GetInstance()->DocumentDetached(document_id, this);
}

}  // namespace content
