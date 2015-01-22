// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/ipc_devtools_agent_host.h"

#include "content/common/devtools_messages.h"

namespace content {

void IPCDevToolsAgentHost::Attach() {
  SendMessageToAgent(new DevToolsAgentMsg_Attach(MSG_ROUTING_NONE, GetId()));
  OnClientAttached();
}

void IPCDevToolsAgentHost::Detach() {
  SendMessageToAgent(new DevToolsAgentMsg_Detach(MSG_ROUTING_NONE));
  OnClientDetached();
}

void IPCDevToolsAgentHost::DispatchProtocolMessage(
    const std::string& message) {
  SendMessageToAgent(new DevToolsAgentMsg_DispatchOnInspectorBackend(
      MSG_ROUTING_NONE, message));
}

void IPCDevToolsAgentHost::InspectElement(int x, int y) {
  SendMessageToAgent(new DevToolsAgentMsg_InspectElement(MSG_ROUTING_NONE,
                                                         GetId(), x, y));
}

IPCDevToolsAgentHost::IPCDevToolsAgentHost()
    : message_buffer_size_(0) {
}

IPCDevToolsAgentHost::~IPCDevToolsAgentHost() {
}

void IPCDevToolsAgentHost::Reattach(const std::string& saved_agent_state) {
  SendMessageToAgent(new DevToolsAgentMsg_Reattach(
      MSG_ROUTING_NONE, GetId(), saved_agent_state));
  OnClientAttached();
}

void IPCDevToolsAgentHost::ProcessChunkedMessageFromAgent(
    const std::string& message, uint32 total_size) {
  if (total_size && total_size == message.length()) {
    DCHECK(message_buffer_size_ == 0);
    SendMessageToClient(message);
    return;
  }

  if (total_size) {
    DCHECK(message_buffer_size_ == 0);
    message_buffer_ = std::string();
    message_buffer_.reserve(total_size);
    message_buffer_size_ = total_size;
  }

  message_buffer_.append(message);

  if (message_buffer_.size() >= message_buffer_size_) {
    DCHECK(message_buffer_.size() == message_buffer_size_);
    SendMessageToClient(message_buffer_);
    message_buffer_ = std::string();
    message_buffer_size_ = 0;
  }
}

}  // namespace content
