// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/render_view_devtools_agent_host.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "content/browser/debugger/devtools_manager_impl.h"
#include "content/browser/debugger/render_view_devtools_agent_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace content {

typedef std::map<RenderViewHost*, RenderViewDevToolsAgentHost*> Instances;

namespace {
base::LazyInstance<Instances,
                   base::LeakyLazyInstanceTraits<Instances> >
    g_instances = LAZY_INSTANCE_INITIALIZER;
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
bool DevToolsAgentHostRegistry::HasDevToolsAgentHost(RenderViewHost* rvh) {
  if (g_instances == NULL)
    return false;
  Instances::iterator it = g_instances.Get().find(rvh);
  return it != g_instances.Get().end();
}

bool DevToolsAgentHostRegistry::IsDebuggerAttached(TabContents* tab_contents) {
  if (g_instances == NULL)
    return false;
  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  if (!devtools_manager)
    return false;
  RenderViewHostDelegate* delegate = tab_contents;
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if (it->first->delegate() != delegate)
      continue;
    if (devtools_manager->GetDevToolsClientHostFor(it->second))
      return true;
  }
  return false;
}

RenderViewDevToolsAgentHost::RenderViewDevToolsAgentHost(RenderViewHost* rvh)
    : content::RenderViewHostObserver(rvh),
      render_view_host_(rvh) {
  g_instances.Get()[rvh] = this;
}

void RenderViewDevToolsAgentHost::SendMessageToAgent(IPC::Message* msg) {
  msg->set_routing_id(render_view_host_->routing_id());
  render_view_host_->Send(msg);
}

void RenderViewDevToolsAgentHost::NotifyClientClosing() {
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_DEVTOOLS_WINDOW_CLOSING,
      content::Source<content::BrowserContext>(
          render_view_host_->site_instance()->GetProcess()->
              GetBrowserContext()),
      content::Details<RenderViewHost>(render_view_host_));
}

int RenderViewDevToolsAgentHost::GetRenderProcessId() {
  return render_view_host_->process()->GetID();
}

RenderViewDevToolsAgentHost::~RenderViewDevToolsAgentHost() {
  g_instances.Get().erase(render_view_host_);
}

void RenderViewDevToolsAgentHost::RenderViewHostDestroyed(RenderViewHost* rvh) {
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
  content::GetContentClient()->browser()->ClearCache(render_view_host_);
}

void RenderViewDevToolsAgentHost::OnClearBrowserCookies() {
  content::GetContentClient()->browser()->ClearCookies(render_view_host_);
}

}  // namespace content

