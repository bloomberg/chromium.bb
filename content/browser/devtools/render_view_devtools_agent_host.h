// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/browser/devtools/devtools_agent_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_view_host_observer.h"

namespace content {

class RenderViewHost;

class CONTENT_EXPORT RenderViewDevToolsAgentHost
    : public DevToolsAgentHost,
      private RenderViewHostObserver {
 public:
  RenderViewDevToolsAgentHost(RenderViewHost*);

 private:
  virtual ~RenderViewDevToolsAgentHost();

  // DevToolsAgentHost implementation.
  virtual void SendMessageToAgent(IPC::Message* msg) OVERRIDE;
  virtual void NotifyClientAttaching() OVERRIDE;
  virtual void NotifyClientDetaching() OVERRIDE;
  virtual int GetRenderProcessId() OVERRIDE;

  // RenderViewHostObserver overrides.
  virtual void RenderViewHostDestroyed(RenderViewHost* rvh) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnDispatchOnInspectorFrontend(const std::string& message);
  void OnSaveAgentRuntimeState(const std::string& state);
  void OnClearBrowserCache();
  void OnClearBrowserCookies();

  bool CaptureScreenshot(std::string* base_64_data);

  RenderViewHost* render_view_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
