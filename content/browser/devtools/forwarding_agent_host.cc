// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/forwarding_agent_host.h"

#include "base/bind.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/inspector_handler.h"

namespace content {

ForwardingAgentHost::ForwardingAgentHost(
    const std::string& id,
    std::unique_ptr<DevToolsExternalAgentProxyDelegate> delegate)
      : DevToolsAgentHostImpl(id),
        delegate_(std::move(delegate)) {
  NotifyCreated();
}

ForwardingAgentHost::~ForwardingAgentHost() {
}

void ForwardingAgentHost::DispatchOnClientHost(const std::string& message) {
  if (sessions().empty())
    return;
  (*sessions().begin())->SendMessageToClient(message);
}

void ForwardingAgentHost::ConnectionClosed() {
  ForceDetachAllClients(false);
}

void ForwardingAgentHost::AttachSession(DevToolsSession* session) {
  delegate_->Attach(this);
}

void ForwardingAgentHost::DetachSession(int session_id) {
  delegate_->Detach();
}

bool ForwardingAgentHost::DispatchProtocolMessage(
    DevToolsSession* session,
    const std::string& message) {
  delegate_->SendMessageToBackend(message);
  return true;
}

std::string ForwardingAgentHost::GetType() {
  return delegate_->GetType();
}

std::string ForwardingAgentHost::GetTitle() {
  return delegate_->GetTitle();
}

GURL ForwardingAgentHost::GetURL() {
  return delegate_->GetURL();
}

GURL ForwardingAgentHost::GetFaviconURL() {
  return delegate_->GetFaviconURL();
}

std::string ForwardingAgentHost::GetFrontendURL() {
  return delegate_->GetFrontendURL();
}

bool ForwardingAgentHost::Activate() {
  return delegate_->Activate();
}

void ForwardingAgentHost::Reload() {
  delegate_->Reload();
}

bool ForwardingAgentHost::Close() {
  return delegate_->Close();
}

base::TimeTicks ForwardingAgentHost::GetLastActivityTime() {
  return delegate_->GetLastActivityTime();
}

}  // content
