// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/geofencing/geofencing_message_filter.h"

#include "content/child/geofencing/geofencing_dispatcher.h"
#include "ipc/ipc_message_macros.h"

namespace content {

GeofencingMessageFilter::GeofencingMessageFilter(ThreadSafeSender* sender)
    : WorkerThreadMessageFilter(sender) {
}

GeofencingMessageFilter::~GeofencingMessageFilter() {
}

bool GeofencingMessageFilter::ShouldHandleMessage(
    const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == GeofencingMsgStart;
}

void GeofencingMessageFilter::OnFilteredMessageReceived(
    const IPC::Message& msg) {
  GeofencingDispatcher::GetOrCreateThreadSpecificInstance(thread_safe_sender())
      ->OnMessageReceived(msg);
}

bool GeofencingMessageFilter::GetWorkerThreadIdForMessage(
    const IPC::Message& msg,
    int* ipc_thread_id) {
  return base::PickleIterator(msg).ReadInt(ipc_thread_id);
}

}  // namespace content
