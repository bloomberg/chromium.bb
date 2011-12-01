// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/worker_devtools_message_filter.h"

#include "content/browser/debugger/worker_devtools_manager.h"
#include "content/browser/worker_host/worker_service.h"
#include "content/common/devtools_messages.h"
#include "content/common/worker_messages.h"

namespace content {

WorkerDevToolsMessageFilter::WorkerDevToolsMessageFilter(
    int worker_process_host_id)
    : worker_process_host_id_(worker_process_host_id),
      current_routing_id_(0) {
}

WorkerDevToolsMessageFilter::~WorkerDevToolsMessageFilter() {
}

bool WorkerDevToolsMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  current_routing_id_ = message.routing_id();
  IPC_BEGIN_MESSAGE_MAP_EX(WorkerDevToolsMessageFilter, message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_SaveAgentRuntimeState,
                        OnSaveAgentRumtimeState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void WorkerDevToolsMessageFilter::OnDispatchOnInspectorFrontend(
    const std::string& message) {
  WorkerDevToolsManager::GetInstance()->ForwardToDevToolsClient(
      worker_process_host_id_, current_routing_id_, message);
}

void WorkerDevToolsMessageFilter::OnSaveAgentRumtimeState(
    const std::string& state) {
  WorkerDevToolsManager::GetInstance()->SaveAgentRuntimeState(
      worker_process_host_id_, current_routing_id_, state);
}

}  // namespace content
