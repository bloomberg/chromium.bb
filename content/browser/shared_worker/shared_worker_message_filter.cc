// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_message_filter.h"

#include <stdint.h>

#include "base/macros.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationErrors.h"

namespace content {
namespace {

const uint32_t kFilteredMessageClasses[] = {
    ViewMsgStart, WorkerMsgStart,
};

}  // namespace

SharedWorkerMessageFilter::SharedWorkerMessageFilter(
    int render_process_id,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition,
    const NextRoutingIDCallback& next_routing_id_callback)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      resource_context_(resource_context),
      partition_(partition),
      next_routing_id_callback_(next_routing_id_callback) {}

SharedWorkerMessageFilter::~SharedWorkerMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void SharedWorkerMessageFilter::OnChannelClosing() {
  SharedWorkerServiceImpl::GetInstance()->OnSharedWorkerMessageFilterClosing(
      this);
}

bool SharedWorkerMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(SharedWorkerMessageFilter, message, this)
    // Only sent from renderer for now, until we have nested workers.
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWorker, OnCreateWorker)
    IPC_MESSAGE_FORWARD(ViewHostMsg_ConnectToWorker,
                        SharedWorkerServiceImpl::GetInstance(),
                        SharedWorkerServiceImpl::ConnectToWorker)
    // Only sent from renderer.
    IPC_MESSAGE_FORWARD(ViewHostMsg_DocumentDetached,
                        SharedWorkerServiceImpl::GetInstance(),
                        SharedWorkerServiceImpl::DocumentDetached)
    // Only sent from SharedWorker in renderer.
    IPC_MESSAGE_FORWARD(WorkerHostMsg_CountFeature,
                        SharedWorkerServiceImpl::GetInstance(),
                        SharedWorkerServiceImpl::CountFeature)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerContextClosed,
                        SharedWorkerServiceImpl::GetInstance(),
                        SharedWorkerServiceImpl::WorkerContextClosed)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerContextDestroyed,
                        SharedWorkerServiceImpl::GetInstance(),
                        SharedWorkerServiceImpl::WorkerContextDestroyed)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerReadyForInspection,
                        SharedWorkerServiceImpl::GetInstance(),
                        SharedWorkerServiceImpl::WorkerReadyForInspection)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerScriptLoaded,
                        SharedWorkerServiceImpl::GetInstance(),
                        SharedWorkerServiceImpl::WorkerScriptLoaded)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerScriptLoadFailed,
                        SharedWorkerServiceImpl::GetInstance(),
                        SharedWorkerServiceImpl::WorkerScriptLoadFailed)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerConnected,
                        SharedWorkerServiceImpl::GetInstance(),
                        SharedWorkerServiceImpl::WorkerConnected)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        WorkerProcessHostMsg_RequestFileSystemAccessSync,
        OnRequestFileSystemAccess)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowIndexedDB, OnAllowIndexedDB)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

int SharedWorkerMessageFilter::GetNextRoutingID() {
  return next_routing_id_callback_.Run();
}

void SharedWorkerMessageFilter::OnCreateWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    ViewHostMsg_CreateWorker_Reply* reply) {
  reply->route_id = GetNextRoutingID();
  reply->error = SharedWorkerServiceImpl::GetInstance()->CreateWorker(
      params, reply->route_id, this, resource_context_,
      WorkerStoragePartitionId(partition_));
}

void SharedWorkerMessageFilter::OnRequestFileSystemAccess(
    int worker_route_id,
    const GURL& url,
    IPC::Message* reply_msg) {
  SharedWorkerServiceImpl::GetInstance()->AllowFileSystem(this, worker_route_id,
                                                          url, reply_msg);
}

void SharedWorkerMessageFilter::OnAllowIndexedDB(int worker_route_id,
                                                 const GURL& url,
                                                 const base::string16& name,
                                                 bool* result) {
  SharedWorkerServiceImpl::GetInstance()->AllowIndexedDB(this, worker_route_id,
                                                         url, name, result);
}

}  // namespace content
