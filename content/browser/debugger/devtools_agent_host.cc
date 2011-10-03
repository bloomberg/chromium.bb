// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_agent_host.h"

#include "base/basictypes.h"
#include "content/common/devtools_messages.h"

DevToolsAgentHost::DevToolsAgentHost() : close_listener_(NULL) {
}

void DevToolsAgentHost::Attach(const DevToolsRuntimeProperties& properties) {
  SendMessageToAgent(new DevToolsAgentMsg_Attach(
      MSG_ROUTING_NONE,
      properties));
}

void DevToolsAgentHost::Detach() {
  SendMessageToAgent(new DevToolsAgentMsg_Detach(MSG_ROUTING_NONE));
}

void DevToolsAgentHost::NotifyCloseListener() {
  if (close_listener_) {
    close_listener_->AgentHostClosing(this);
    close_listener_ = NULL;
  }
}

