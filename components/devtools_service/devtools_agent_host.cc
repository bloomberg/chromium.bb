// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_service/devtools_agent_host.h"

#include "base/guid.h"
#include "base/logging.h"

namespace devtools_service {

DevToolsAgentHost::DevToolsAgentHost(DevToolsAgentPtr agent)
    : id_(base::GenerateGUID()),
      agent_(agent.Pass()),
      binding_(this),
      delegate_(nullptr) {
  agent_.set_error_handler(this);
}

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
    agent_->SetClient(client.Pass(), id_);
  } else {
    if (!binding_.is_bound())
      return;

    binding_.Close();
  }
}

void DevToolsAgentHost::SendProtocolMessageToAgent(const std::string& message) {
  agent_->DispatchProtocolMessage(message);
}

void DevToolsAgentHost::DispatchProtocolMessage(const mojo::String& message) {
  delegate_->DispatchProtocolMessage(this, message);
}

void DevToolsAgentHost::OnConnectionError() {
  agent_connection_error_handler_.Run();
}

}  // namespace devtools_service
