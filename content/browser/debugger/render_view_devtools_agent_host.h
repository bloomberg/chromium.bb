// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEBUGGER_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "content/browser/debugger/devtools_agent_host.h"
#include "content/browser/renderer_host/render_view_host_observer.h"

class RenderViewHost;

class RenderViewDevToolsAgentHost : public DevToolsAgentHost,
                                    private RenderViewHostObserver {
 public:
  static DevToolsAgentHost* FindFor(RenderViewHost*);

 private:
  RenderViewDevToolsAgentHost(RenderViewHost*);
  virtual ~RenderViewDevToolsAgentHost();

  // DevToolsAgentHost implementation.
  virtual void SendMessageToAgent(IPC::Message* msg);
  virtual void NotifyClientClosing();
  virtual int GetRenderProcessId();

  // RenderViewHostObserver overrides.
  virtual void RenderViewHostDestroyed() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnForwardToClient(const IPC::Message& message);
  void OnRuntimePropertyChanged(const std::string& name,
                                const std::string& value);
  void OnClearBrowserCache();
  void OnClearBrowserCookies();

  RenderViewHost* render_view_host_;

  typedef std::map<RenderViewHost*, RenderViewDevToolsAgentHost*> Instances;
  static Instances instances_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewDevToolsAgentHost);
};

#endif  // CONTENT_BROWSER_DEBUGGER_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
