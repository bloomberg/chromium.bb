// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_AGENT_HOST_H_
#pragma once

#include <string>

#include "content/common/content_export.h"

namespace IPC {
class Message;
}

namespace content {

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHost {
 public:
  class CONTENT_EXPORT CloseListener {
   public:
    virtual void AgentHostClosing(DevToolsAgentHost*) = 0;
   protected:
    virtual ~CloseListener() {}
  };

  // Sends the message to the devtools agent hosted by this object.
  void Attach();
  void Reattach(const std::string& saved_agent_state);
  void Detach();
  void DipatchOnInspectorBackend(const std::string& message);
  void InspectElement(int x, int y);

  // TODO(yurys): get rid of this method
  virtual void NotifyClientClosing() = 0;

  virtual int GetRenderProcessId() = 0;

  void set_close_listener(CloseListener* listener) {
    close_listener_ = listener;
  }

 protected:
  DevToolsAgentHost();
  virtual ~DevToolsAgentHost() {}

  virtual void SendMessageToAgent(IPC::Message* msg) = 0;
  void NotifyCloseListener();

  CloseListener* close_listener_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_AGENT_HOST_H_
