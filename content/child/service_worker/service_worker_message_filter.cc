// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_message_filter.h"

#include <stddef.h>

#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_message_macros.h"

namespace content {

namespace {

// Sends a ServiceWorkerObjectDestroyed message to the browser so it can delete
// the ServiceWorker handle.
void SendServiceWorkerObjectDestroyed(
    ThreadSafeSender* sender,
    int handle_id) {
  if (handle_id == kInvalidServiceWorkerHandleId)
    return;
  sender->Send(
      new ServiceWorkerHostMsg_DecrementServiceWorkerRefCount(handle_id));
}

}  // namespace

ServiceWorkerMessageFilter::ServiceWorkerMessageFilter(ThreadSafeSender* sender)
    : WorkerThreadMessageFilter(sender) {
}

ServiceWorkerMessageFilter::~ServiceWorkerMessageFilter() {}

bool ServiceWorkerMessageFilter::ShouldHandleMessage(
    const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == ServiceWorkerMsgStart;
}

void ServiceWorkerMessageFilter::OnFilteredMessageReceived(
    const IPC::Message& msg) {
  ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
      thread_safe_sender(), main_thread_task_runner())
      ->OnMessageReceived(msg);
}

bool ServiceWorkerMessageFilter::GetWorkerThreadIdForMessage(
    const IPC::Message& msg,
    int* ipc_thread_id) {
  return base::PickleIterator(msg).ReadInt(ipc_thread_id);
}

void ServiceWorkerMessageFilter::OnStaleMessageReceived(
    const IPC::Message& msg) {
  // Specifically handle some messages in case we failed to post task
  // to the thread (meaning that the context on the thread is now gone).
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerMessageFilter, msg)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SetVersionAttributes,
                        OnStaleSetVersionAttributes)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SetControllerServiceWorker,
                        OnStaleSetControllerServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_MessageToDocument,
                        OnStaleMessageToDocument)
  IPC_END_MESSAGE_MAP()
}

void ServiceWorkerMessageFilter::OnStaleSetVersionAttributes(
    int thread_id,
    int registration_handle_id,
    int changed_mask,
    const ServiceWorkerVersionAttributes& attrs) {
  SendServiceWorkerObjectDestroyed(thread_safe_sender(),
                                   attrs.installing.handle_id);
  SendServiceWorkerObjectDestroyed(thread_safe_sender(),
                                   attrs.waiting.handle_id);
  SendServiceWorkerObjectDestroyed(thread_safe_sender(),
                                   attrs.active.handle_id);
  // Don't have to decrement registration refcount because the sender of the
  // SetVersionAttributes message doesn't increment it.
}

void ServiceWorkerMessageFilter::OnStaleSetControllerServiceWorker(
    const ServiceWorkerMsg_SetControllerServiceWorker_Params& params) {
  SendServiceWorkerObjectDestroyed(thread_safe_sender(),
                                   params.object_info.handle_id);
}

void ServiceWorkerMessageFilter::OnStaleMessageToDocument(
    const ServiceWorkerMsg_MessageToDocument_Params& params) {
  SendServiceWorkerObjectDestroyed(thread_safe_sender(),
                                   params.service_worker_info.handle_id);
}

}  // namespace content
