// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

class DevToolsAgentHost;
class DevToolsExternalAgentProxyDelegate;

// Describes interface for communication with an external DevTools agent.
class DevToolsExternalAgentProxy {
 public:
  // Creates DevToolsExternalAgentProxy to communicate with an agent
  // via the provided |delegate|.
  // Caller get the proxy ownership and keeps the |delegate| ownership.
  static CONTENT_EXPORT DevToolsExternalAgentProxy* Create(
      DevToolsExternalAgentProxyDelegate* delegate);

  // Returns the local DevToolsAgentHost for the external agent.
  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() = 0;

  // Sends the message to the client host.
  virtual void DispatchOnClientHost(const std::string& message) = 0;

  // Informs the client that the connection has closed.
  virtual void ConnectionClosed() = 0;

  virtual ~DevToolsExternalAgentProxy() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_H_
