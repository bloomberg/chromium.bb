// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/render_view_devtools_agent_host.h"

#include "base/basictypes.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/debugger/render_view_devtools_agent_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/common/devtools_messages.h"
#include "content/common/notification_service.h"

RenderViewDevToolsAgentHost::Instances RenderViewDevToolsAgentHost::instances_;

DevToolsAgentHost* RenderViewDevToolsAgentHost::FindFor(
    RenderViewHost* rvh) {
  Instances::iterator it = instances_.find(rvh);
  if (it != instances_.end())
    return it->second;
  return new RenderViewDevToolsAgentHost(rvh);
}

RenderViewDevToolsAgentHost::RenderViewDevToolsAgentHost(RenderViewHost* rvh)
    : RenderViewHostObserver(rvh),
      render_view_host_(rvh) {
  instances_[rvh] = this;
}

void RenderViewDevToolsAgentHost::SendMessageToAgent(IPC::Message* msg) {
  msg->set_routing_id(render_view_host_->routing_id());
  render_view_host_->Send(msg);
}

void RenderViewDevToolsAgentHost::NotifyClientClosing() {
  NotificationService::current()->Notify(
      content::NOTIFICATION_DEVTOOLS_WINDOW_CLOSING,
      Source<content::BrowserContext>(
          render_view_host_->site_instance()->GetProcess()->browser_context()),
      Details<RenderViewHost>(render_view_host_));
}

int RenderViewDevToolsAgentHost::GetRenderProcessId() {
  return render_view_host_->process()->id();
}

RenderViewDevToolsAgentHost::~RenderViewDevToolsAgentHost() {
  instances_.erase(render_view_host_);
}

void RenderViewDevToolsAgentHost::RenderViewHostDestroyed() {
  NotifyCloseListener();
  delete this;
}

bool RenderViewDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewDevToolsAgentHost, message)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ForwardToClient, OnForwardToClient)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_RuntimePropertyChanged,
                        OnRuntimePropertyChanged)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ClearBrowserCache, OnClearBrowserCache)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ClearBrowserCookies,
                        OnClearBrowserCookies)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderViewDevToolsAgentHost::OnRuntimePropertyChanged(
    const std::string& name,
    const std::string& value) {
  DevToolsManager::GetInstance()->RuntimePropertyChanged(
      this, name, value);
}

void RenderViewDevToolsAgentHost::OnForwardToClient(
    const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsClient(
      this, message);
}

void RenderViewDevToolsAgentHost::OnClearBrowserCache() {
  content::GetContentClient()->browser()->ClearCache(render_view_host_);
}

void RenderViewDevToolsAgentHost::OnClearBrowserCookies() {
  content::GetContentClient()->browser()->ClearCookies(render_view_host_);
}

