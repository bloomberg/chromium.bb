// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_IPC_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_IPC_DEVTOOLS_AGENT_HOST_H_

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/devtools_messages.h"

namespace IPC {
class Message;
}

namespace content {

class CONTENT_EXPORT IPCDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  // DevToolsAgentHostImpl implementation.
  void Attach() override;
  void Detach() override;
  void DispatchProtocolMessage(const std::string& message) override;
  void InspectElement(int x, int y) override;

 protected:
  IPCDevToolsAgentHost();
  ~IPCDevToolsAgentHost() override;

  void Reattach();
  void ProcessChunkedMessageFromAgent(const DevToolsMessageChunk& chunk);

  virtual void SendMessageToAgent(IPC::Message* msg) = 0;
  virtual void OnClientAttached() = 0;
  virtual void OnClientDetached() = 0;

 private:
  std::string message_buffer_;
  uint32 message_buffer_size_;
  std::string state_cookie_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_IPC_DEVTOOLS_AGENT_HOST_H_
