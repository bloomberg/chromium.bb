// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/worker_devtools_message_filter.h"

#include "content/browser/debugger/worker_devtools_manager.h"
#include "content/browser/worker_host/worker_service.h"
#include "content/common/devtools_messages.h"
#include "content/common/worker_messages.h"

WorkerDevToolsMessageFilter::WorkerDevToolsMessageFilter(
    int worker_process_host_id)
    : worker_process_host_id_(worker_process_host_id) {
}

WorkerDevToolsMessageFilter::~WorkerDevToolsMessageFilter() {
}

void WorkerDevToolsMessageFilter::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();
  WorkerDevToolsManager::GetInstance()->WorkerProcessDestroying(
      worker_process_host_id_);
}

bool WorkerDevToolsMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WorkerDevToolsMessageFilter, message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ForwardToClient, OnForwardToClient)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void WorkerDevToolsMessageFilter::OnForwardToClient(
    const IPC::Message& message) {
  WorkerDevToolsManager::GetInstance()->ForwardToDevToolsClient(
      worker_process_host_id_, message.routing_id(), message);
}
