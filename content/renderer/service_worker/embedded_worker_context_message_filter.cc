// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_context_message_filter.h"

#include "content/child/child_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_context_client.h"
#include "ipc/ipc_message_macros.h"

namespace content {

EmbeddedWorkerContextMessageFilter::EmbeddedWorkerContextMessageFilter()
    : WorkerThreadMessageFilter(
          ChildThreadImpl::current()->thread_safe_sender()) {
}

EmbeddedWorkerContextMessageFilter::~EmbeddedWorkerContextMessageFilter() {}

bool EmbeddedWorkerContextMessageFilter::ShouldHandleMessage(
    const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == EmbeddedWorkerContextMsgStart;
}

void EmbeddedWorkerContextMessageFilter::OnFilteredMessageReceived(
    const IPC::Message& msg) {
  EmbeddedWorkerContextClient* client =
      EmbeddedWorkerContextClient::ThreadSpecificInstance();
  if (!client)
    LOG(ERROR) << "Stray message is sent to nonexistent worker";
  else
    client->OnMessageReceived(msg);
}

bool EmbeddedWorkerContextMessageFilter::GetWorkerThreadIdForMessage(
    const IPC::Message& msg,
    int* ipc_thread_id) {
  return PickleIterator(msg).ReadInt(ipc_thread_id);
}

}  // namespace content
