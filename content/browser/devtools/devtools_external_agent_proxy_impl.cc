// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_external_agent_proxy_impl.h"

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"

namespace content {

class DevToolsExternalAgentProxyImpl::ForwardingAgentHost
    : public DevToolsAgentHostImpl {
 public:
  ForwardingAgentHost(DevToolsExternalAgentProxyDelegate* delegate)
      : delegate_(delegate) {
  }

  void ConnectionClosed() {
    NotifyCloseListener();
  }

 private:
  ~ForwardingAgentHost() {
  }

  // DevToolsAgentHostImpl implementation.
  virtual void Attach() OVERRIDE {
    delegate_->Attach();
  };

  virtual void Detach() OVERRIDE {
    delegate_->Detach();
  };

  virtual void DispatchOnInspectorBackend(const std::string& message) OVERRIDE {
    delegate_->SendMessageToBackend(message);
  }

  DevToolsExternalAgentProxyDelegate* delegate_;
};

//static
DevToolsExternalAgentProxy* DevToolsExternalAgentProxy::Create(
    DevToolsExternalAgentProxyDelegate* delegate) {
  return new DevToolsExternalAgentProxyImpl(delegate);
}

DevToolsExternalAgentProxyImpl::DevToolsExternalAgentProxyImpl(
    DevToolsExternalAgentProxyDelegate* delegate)
    : agent_host_(new ForwardingAgentHost(delegate)) {
}

DevToolsExternalAgentProxyImpl::~DevToolsExternalAgentProxyImpl() {
}

scoped_refptr<DevToolsAgentHost> DevToolsExternalAgentProxyImpl::
    GetAgentHost() {
  return agent_host_;
}

void DevToolsExternalAgentProxyImpl::ConnectionClosed() {
  agent_host_->ConnectionClosed();
}

}  // content
