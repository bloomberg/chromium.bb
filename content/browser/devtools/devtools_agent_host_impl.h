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

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  class CONTENT_EXPORT CloseListener {
   public:
    virtual void AgentHostClosing(DevToolsAgentHostImpl*) = 0;
   protected:
    virtual ~CloseListener() {}
  };

  // Sends the message to the devtools agent hosted by this object.
  void Attach();
  void Reattach(const std::string& saved_agent_state);
  void Detach();
  virtual void DispatchOnInspectorBackend(const std::string& message);

  void set_close_listener(CloseListener* listener) {
    close_listener_ = listener;
  }

  // DevToolsAgentHost implementation.
  virtual void InspectElement(int x, int y) OVERRIDE;

  virtual std::string GetId() OVERRIDE;

  virtual RenderViewHost* GetRenderViewHost() OVERRIDE;

 protected:
  DevToolsAgentHostImpl();
  virtual ~DevToolsAgentHostImpl();

  virtual void SendMessageToAgent(IPC::Message* msg) = 0;
  virtual void NotifyClientAttaching() = 0;
  virtual void NotifyClientDetaching() = 0;

  void NotifyCloseListener();

 private:
  CloseListener* close_listener_;
  const std::string id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
