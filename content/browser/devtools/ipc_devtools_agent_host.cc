// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/ipc_devtools_agent_host.h"

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

void IPCDevToolsAgentHost::Reattach() {
  SendMessageToAgent(new DevToolsAgentMsg_Reattach(
      MSG_ROUTING_NONE, GetId(), state_cookie_));
  OnClientAttached();
}

void IPCDevToolsAgentHost::ProcessChunkedMessageFromAgent(
    const DevToolsMessageChunk& chunk) {
  if (chunk.is_last && !chunk.post_state.empty())
    state_cookie_ = chunk.post_state;

  if (chunk.is_first && chunk.is_last) {
    CHECK(message_buffer_size_ == 0);
    SendMessageToClient(chunk.data);
    return;
  }

  if (chunk.is_first) {
    message_buffer_ = std::string();
    message_buffer_.reserve(chunk.message_size);
    message_buffer_size_ = chunk.message_size;
  }

  CHECK(message_buffer_.size() + chunk.data.size() <=
      message_buffer_size_);
  message_buffer_.append(chunk.data);

  if (chunk.is_last) {
    CHECK(message_buffer_.size() == message_buffer_size_);
    SendMessageToClient(message_buffer_);
    message_buffer_ = std::string();
    message_buffer_size_ = 0;
  }
}

}  // namespace content
