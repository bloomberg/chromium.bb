// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_view_devtools_agent_host.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/devtools/render_view_devtools_agent_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace content {

typedef std::map<RenderViewHost*, RenderViewDevToolsAgentHost*> Instances;

namespace {
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
DevToolsAgentHost* DevToolsAgentHostRegistry::GetDevToolsAgentHost(
    RenderViewHost* rvh) {
  Instances::iterator it = g_instances.Get().find(rvh);
  if (it != g_instances.Get().end())
    return it->second;
  return new RenderViewDevToolsAgentHost(rvh);
}

// static
RenderViewHost* DevToolsAgentHostRegistry::GetRenderViewHost(
     DevToolsAgentHost* agent_host) {
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if (agent_host == it->second)
      return it->first;
  }
  return NULL;
}

// static
bool DevToolsAgentHostRegistry::HasDevToolsAgentHost(RenderViewHost* rvh) {
  if (g_instances == NULL)
    return false;
  Instances::iterator it = g_instances.Get().find(rvh);
  return it != g_instances.Get().end();
}

bool DevToolsAgentHostRegistry::IsDebuggerAttached(WebContents* web_contents) {
  if (g_instances == NULL)
    return false;
  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  if (!devtools_manager)
    return false;
  RenderViewHostDelegate* delegate =
      static_cast<WebContentsImpl*>(web_contents);
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if (it->first->GetDelegate() != delegate)
      continue;
    if (devtools_manager->GetDevToolsClientHostFor(it->second))
      return true;
  }
  return false;
}

RenderViewDevToolsAgentHost::RenderViewDevToolsAgentHost(
    RenderViewHost* rvh)
    : RenderViewHostObserver(rvh),
      render_view_host_(rvh) {
  g_instances.Get()[rvh] = this;
}

void RenderViewDevToolsAgentHost::SendMessageToAgent(IPC::Message* msg) {
  msg->set_routing_id(render_view_host_->GetRoutingID());
  render_view_host_->Send(msg);
}

void RenderViewDevToolsAgentHost::NotifyClientAttaching() {
  NotificationService::current()->Notify(
      NOTIFICATION_DEVTOOLS_AGENT_ATTACHED,
      Source<BrowserContext>(
          render_view_host_->GetSiteInstance()->GetProcess()->
              GetBrowserContext()),
      Details<RenderViewHost>(render_view_host_));
}

void RenderViewDevToolsAgentHost::NotifyClientDetaching() {
  NotificationService::current()->Notify(
      NOTIFICATION_DEVTOOLS_AGENT_DETACHED,
      Source<BrowserContext>(
          render_view_host_->GetSiteInstance()->GetProcess()->
              GetBrowserContext()),
      Details<RenderViewHost>(render_view_host_));
}

int RenderViewDevToolsAgentHost::GetRenderProcessId() {
  return render_view_host_->GetProcess()->GetID();
}

RenderViewDevToolsAgentHost::~RenderViewDevToolsAgentHost() {
  g_instances.Get().erase(render_view_host_);
}

void RenderViewDevToolsAgentHost::RenderViewHostDestroyed(
    RenderViewHost* rvh) {
  NotifyCloseListener();
  delete this;
}

bool RenderViewDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewDevToolsAgentHost, message)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_SaveAgentRuntimeState,
                        OnSaveAgentRuntimeState)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ClearBrowserCache, OnClearBrowserCache)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ClearBrowserCookies,
                        OnClearBrowserCookies)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderViewDevToolsAgentHost::OnSaveAgentRuntimeState(
    const std::string& state) {
  DevToolsManagerImpl::GetInstance()->SaveAgentRuntimeState(this, state);
}

void RenderViewDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    const std::string& message) {
  DevToolsManagerImpl::GetInstance()->DispatchOnInspectorFrontend(
      this, message);
}

void RenderViewDevToolsAgentHost::OnClearBrowserCache() {
  GetContentClient()->browser()->ClearCache(render_view_host_);
}

void RenderViewDevToolsAgentHost::OnClearBrowserCookies() {
  GetContentClient()->browser()->ClearCookies(render_view_host_);
}

}  // namespace content
