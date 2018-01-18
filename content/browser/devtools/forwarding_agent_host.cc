// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/forwarding_agent_host.h"

#include "base/bind.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"

namespace content {

class ForwardingAgentHost::SessionProxy : public DevToolsExternalAgentProxy {
 public:
  SessionProxy(ForwardingAgentHost* agent_host, DevToolsSession* session)
      : agent_host_(agent_host), session_(session) {
    agent_host_->delegate_->Attach(this);
  }

  ~SessionProxy() override { agent_host_->delegate_->Detach(this); }

 private:
  void DispatchOnClientHost(const std::string& message) override {
    session_->client()->DispatchProtocolMessage(agent_host_, message);
  }

  void ConnectionClosed() override {
    agent_host_->ForceDetachSession(session_);
  }

  ForwardingAgentHost* agent_host_;
  DevToolsSession* session_;

  DISALLOW_COPY_AND_ASSIGN(SessionProxy);
};

ForwardingAgentHost::ForwardingAgentHost(
    const std::string& id,
    std::unique_ptr<DevToolsExternalAgentProxyDelegate> delegate)
      : DevToolsAgentHostImpl(id),
        delegate_(std::move(delegate)) {
  NotifyCreated();
}

ForwardingAgentHost::~ForwardingAgentHost() {
}

void ForwardingAgentHost::AttachSession(DevToolsSession* session) {
  session_proxies_[session].reset(new SessionProxy(this, session));
}

void ForwardingAgentHost::DetachSession(DevToolsSession* session) {
  session_proxies_.erase(session);
}

bool ForwardingAgentHost::DispatchProtocolMessage(
    DevToolsSession* session,
    const std::string& message) {
  auto it = session_proxies_.find(session);
  if (it != session_proxies_.end())
    delegate_->SendMessageToBackend(it->second.get(), message);
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
