// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_message_filter.h"

#include <stddef.h>

#include "content/common/service_worker/service_worker_messages.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "ipc/ipc_message_macros.h"

namespace content {

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

}  // namespace content
