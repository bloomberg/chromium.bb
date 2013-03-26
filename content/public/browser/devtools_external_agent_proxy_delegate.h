// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

// Describes the interface for sending messages to an external DevTools agent.
class DevToolsExternalAgentProxyDelegate {
 public:
   // Informs the agent that a client host has attached.
   virtual void Attach() = 0;

   // Informs the agent that a client host has detached.
   virtual void Detach() = 0;

   // Sends a message to the agent.
   virtual void SendMessageToBackend(const std::string& message) = 0;

 protected:
  virtual ~DevToolsExternalAgentProxyDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_EXTERNAL_AGENT_PROXY_DELEGATE_H_
