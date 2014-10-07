// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host.h"

namespace IPC {
class Message;
}

namespace content {

class BrowserContext;

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  // Returns a list of all existing WebContents that can be debugged.
  static std::vector<WebContents*> GetInspectableWebContents();

  // Informs the hosted agent that a client host has attached.
  virtual void Attach() = 0;

  // Informs the hosted agent that a client host has detached.
  virtual void Detach() = 0;

  // Sends a message to the agent.
  virtual void DispatchProtocolMessage(const std::string& message) = 0;

  // Opens the inspector for this host.
  void Inspect(BrowserContext* browser_context);

  // DevToolsAgentHost implementation.
  virtual void AttachClient(DevToolsAgentHostClient* client) override;
  virtual void DetachClient() override;
  virtual bool IsAttached() override;
  virtual void InspectElement(int x, int y) override;
  virtual std::string GetId() override;
  virtual WebContents* GetWebContents() override;
  virtual void DisconnectWebContents() override;
  virtual void ConnectWebContents(WebContents* wc) override;
  virtual bool IsWorker() const override;

 protected:
  DevToolsAgentHostImpl();
  virtual ~DevToolsAgentHostImpl();

  void HostClosed();
  void SendMessageToClient(const std::string& message);
  static void NotifyCallbacks(DevToolsAgentHostImpl* agent_host, bool attached);

 private:
  friend class DevToolsAgentHost; // for static methods

  const std::string id_;
  DevToolsAgentHostClient* client_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
