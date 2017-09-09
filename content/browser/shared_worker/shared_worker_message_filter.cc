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

namespace content {
namespace {

const uint32_t kFilteredMessageClasses[] = {
    WorkerMsgStart,
};

}  // namespace

SharedWorkerMessageFilter::SharedWorkerMessageFilter(
    int render_process_id,
    const NextRoutingIDCallback& next_routing_id_callback)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      next_routing_id_callback_(next_routing_id_callback) {}

int SharedWorkerMessageFilter::GetNextRoutingID() {
  return next_routing_id_callback_.Run();
}

SharedWorkerMessageFilter::~SharedWorkerMessageFilter() {}

void SharedWorkerMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  SharedWorkerServiceImpl::GetInstance()->AddFilter(this);
}

void SharedWorkerMessageFilter::OnFilterRemoved() {
  SharedWorkerServiceImpl::GetInstance()->RemoveFilter(this);
}

void SharedWorkerMessageFilter::OnChannelClosing() {
  SharedWorkerServiceImpl::GetInstance()->OnSharedWorkerMessageFilterClosing(
      this);
}

bool SharedWorkerMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(SharedWorkerMessageFilter, message, this)
    // Sent from SharedWorker in renderer.
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace content
