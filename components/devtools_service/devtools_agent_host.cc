// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_service/devtools_agent_host.h"

#include <utility>

namespace devtools_service {

DevToolsAgentHost::DevToolsAgentHost(const std::string& id,
                                     DevToolsAgentPtr agent)
    : id_(id), agent_(std::move(agent)), binding_(this), delegate_(nullptr) {}

DevToolsAgentHost::~DevToolsAgentHost() {
  if (delegate_)
    delegate_->OnAgentHostClosed(this);
}

void DevToolsAgentHost::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (delegate_) {
    if (binding_.is_bound())
      return;

    DevToolsAgentClientPtr client;
    binding_.Bind(&client);
    agent_->SetClient(std::move(client));
  } else {
    if (!binding_.is_bound())
      return;

    binding_.Close();
  }
}

void DevToolsAgentHost::SendProtocolMessageToAgent(const std::string& message) {
  agent_->DispatchProtocolMessage(message);
}

void DevToolsAgentHost::DispatchProtocolMessage(int32_t call_id,
                                                const mojo::String& message,
                                                const mojo::String& state) {
  delegate_->DispatchProtocolMessage(this, message);
}

}  // namespace devtools_service
