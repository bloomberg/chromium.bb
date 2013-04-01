// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_EXTERNAL_AGENT_PROXY_IMPL_H
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_EXTERNAL_AGENT_PROXY_IMPL_H

#include "base/memory/ref_counted.h"
#include "content/public/browser/devtools_external_agent_proxy.h"

namespace content {

class DevToolsExternalAgentProxyImpl
    : public DevToolsExternalAgentProxy {
 public:
  explicit DevToolsExternalAgentProxyImpl(
      DevToolsExternalAgentProxyDelegate* delegate);
  virtual ~DevToolsExternalAgentProxyImpl();

  // DevToolsExternalAgentProxy implementation.
  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() OVERRIDE;
  virtual void DispatchOnClientHost(const std::string& message) OVERRIDE;
  virtual void ConnectionClosed() OVERRIDE;

 private:
  class ForwardingAgentHost;

  scoped_refptr<ForwardingAgentHost> agent_host_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsExternalAgentProxyImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_EXTERNAL_AGENT_PROXY_IMPL_H
