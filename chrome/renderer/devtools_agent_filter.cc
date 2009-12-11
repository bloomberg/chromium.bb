// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/devtools_agent_filter.h"

#include "base/message_loop.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/devtools_agent.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsMessageData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/devtools/devtools_message_data.h"

using WebKit::WebDevToolsAgent;
using WebKit::WebString;

// static
void DevToolsAgentFilter::DispatchMessageLoop() {
  MessageLoop* current = MessageLoop::current();
  bool old_state = current->NestableTasksAllowed();
  current->SetNestableTasksAllowed(true);
  current->RunAllPending();
  current->SetNestableTasksAllowed(old_state);
}

// static
IPC::Channel* DevToolsAgentFilter::channel_ = NULL;
// static
int DevToolsAgentFilter::current_routing_id_ = 0;

DevToolsAgentFilter::DevToolsAgentFilter()
    : message_handled_(false) {
  WebDevToolsAgent::setMessageLoopDispatchHandler(
      &DevToolsAgentFilter::DispatchMessageLoop);
}

DevToolsAgentFilter::~DevToolsAgentFilter() {
}

bool DevToolsAgentFilter::OnMessageReceived(const IPC::Message& message) {
  // Dispatch debugger commands directly from IO.
  message_handled_ = true;
  current_routing_id_ = message.routing_id();
  IPC_BEGIN_MESSAGE_MAP(DevToolsAgentFilter, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebuggerCommand, OnDebuggerCommand)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebuggerPauseScript,
                        OnDebuggerPauseScript)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_RpcMessage, OnRpcMessage)
    IPC_MESSAGE_UNHANDLED(message_handled_ = false)
  IPC_END_MESSAGE_MAP()
  return message_handled_;
}

void DevToolsAgentFilter::OnDebuggerCommand(const std::string& command) {
  WebDevToolsAgent::executeDebuggerCommand(
      WebString::fromUTF8(command), current_routing_id_);
}

void DevToolsAgentFilter::OnDebuggerPauseScript() {
  WebDevToolsAgent::debuggerPauseScript();
}

void DevToolsAgentFilter::OnRpcMessage(const DevToolsMessageData& data) {
  message_handled_ = WebDevToolsAgent::dispatchMessageFromFrontendOnIOThread(
      data.ToWebDevToolsMessageData());
}

// static
void DevToolsAgentFilter::SendRpcMessage(const DevToolsMessageData& data) {
  IPC::Message* m = new ViewHostMsg_ForwardToDevToolsClient(
      current_routing_id_,
      DevToolsClientMsg_RpcMessage(data));
  channel_->Send(m);
}
