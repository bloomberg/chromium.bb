// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/devtools_agent_host.h"

namespace IPC {
class Message;
}

namespace content {

class BrowserContext;

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  // Informs the hosted agent that a client host has attached.
  virtual void Attach() = 0;

  // Informs the hosted agent that a client host has detached.
  virtual void Detach() = 0;

  // Opens the inspector for this host.
  void Inspect(BrowserContext* browser_context);

  // DevToolsAgentHost implementation.
  void AttachClient(DevToolsAgentHostClient* client) override;
  void DetachClient() override;
  bool IsAttached() override;
  void InspectElement(int x, int y) override;
  std::string GetId() override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* wc) override;

 protected:
  DevToolsAgentHostImpl();
  ~DevToolsAgentHostImpl() override;

  std::string state_cookie_;

  void HostClosed();
  void SendMessageToClient(const std::string& message);
  void ProcessChunkedMessageFromAgent(const DevToolsMessageChunk& chunk);
  static void NotifyCallbacks(DevToolsAgentHostImpl* agent_host, bool attached);

 private:
  friend class DevToolsAgentHost; // for static methods

  const std::string id_;
  DevToolsAgentHostClient* client_;
  std::string message_buffer_;
  uint32 message_buffer_size_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
