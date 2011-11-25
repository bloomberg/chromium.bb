// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEBUGGER_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "content/browser/debugger/devtools_agent_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_view_host_observer.h"

class RenderViewHost;
class TabContents;

class CONTENT_EXPORT RenderViewDevToolsAgentHost
    : public DevToolsAgentHost,
      private content::RenderViewHostObserver {
 public:
  static DevToolsAgentHost* FindFor(RenderViewHost*);
  static bool IsDebuggerAttached(TabContents*);

 private:
  RenderViewDevToolsAgentHost(RenderViewHost*);
  virtual ~RenderViewDevToolsAgentHost();

  // DevToolsAgentHost implementation.
  virtual void SendMessageToAgent(IPC::Message* msg) OVERRIDE;
  virtual void NotifyClientClosing() OVERRIDE;
  virtual int GetRenderProcessId() OVERRIDE;

  // content::RenderViewHostObserver overrides.
  virtual void RenderViewHostDestroyed(RenderViewHost* rvh) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnForwardToClient(const IPC::Message& message);
  void OnSaveAgentRuntimeState(const std::string& state);
  void OnClearBrowserCache();
  void OnClearBrowserCookies();

  RenderViewHost* render_view_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewDevToolsAgentHost);
};

#endif  // CONTENT_BROWSER_DEBUGGER_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
