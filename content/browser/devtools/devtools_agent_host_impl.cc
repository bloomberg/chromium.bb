// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include "base/basictypes.h"
#include "content/common/devtools_messages.h"

namespace content {

namespace {
static int g_next_agent_host_id = 0;
}  // namespace

DevToolsAgentHostImpl::DevToolsAgentHostImpl()
    : close_listener_(NULL),
      id_(++g_next_agent_host_id) {
}

void DevToolsAgentHostImpl::Attach() {
  SendMessageToAgent(new DevToolsAgentMsg_Attach(MSG_ROUTING_NONE));
  NotifyClientAttaching();
}

void DevToolsAgentHostImpl::Reattach(const std::string& saved_agent_state) {
  SendMessageToAgent(new DevToolsAgentMsg_Reattach(
      MSG_ROUTING_NONE,
      saved_agent_state));
  NotifyClientAttaching();
}

void DevToolsAgentHostImpl::Detach() {
  SendMessageToAgent(new DevToolsAgentMsg_Detach(MSG_ROUTING_NONE));
  NotifyClientDetaching();
}

void DevToolsAgentHostImpl::DispatchOnInspectorBackend(
    const std::string& message) {
  SendMessageToAgent(new DevToolsAgentMsg_DispatchOnInspectorBackend(
      MSG_ROUTING_NONE, message));
}

void DevToolsAgentHostImpl::InspectElement(int x, int y) {
  SendMessageToAgent(new DevToolsAgentMsg_InspectElement(MSG_ROUTING_NONE,
                                                         x, y));
}

void DevToolsAgentHostImpl::AddMessageToConsole(ConsoleMessageLevel level,
                                                const std::string& message) {
  SendMessageToAgent(new DevToolsAgentMsg_AddMessageToConsole(
      MSG_ROUTING_NONE,
      level,
      message));
}

RenderViewHost* DevToolsAgentHostImpl::GetRenderViewHost() {
  return NULL;
}

void DevToolsAgentHostImpl::NotifyCloseListener() {
  if (close_listener_) {
    scoped_refptr<DevToolsAgentHostImpl> protect(this);
    close_listener_->AgentHostClosing(this);
    close_listener_ = NULL;
  }
}

}  // namespace content
