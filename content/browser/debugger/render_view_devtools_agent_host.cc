// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/render_view_devtools_agent_host.h"

#include "base/basictypes.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/common/notification_service.h"

RenderViewDevToolsAgentHost::Instances RenderViewDevToolsAgentHost::instances_;

DevToolsAgentHost* RenderViewDevToolsAgentHost::FindFor(
    RenderViewHost* rvh) {
  Instances::iterator it = instances_.find(rvh);
  if (it != instances_.end())
    return it->second;
  return NULL;
}

DevToolsAgentHost* RenderViewDevToolsAgentHost::CreateFor(
    RenderViewHost* rvh) {
  DCHECK(!FindFor(rvh));
  RenderViewDevToolsAgentHost* result = new RenderViewDevToolsAgentHost(rvh);
  return result;
}

RenderViewDevToolsAgentHost::RenderViewDevToolsAgentHost(RenderViewHost* rvh)
    : render_view_host_(rvh) {
  ChildProcessSecurityPolicy::GetInstance()->GrantReadRawCookies(
      rvh->process()->id());
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
                 Source<RenderViewHost>(rvh));
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

void RenderViewDevToolsAgentHost::ClientDetached() {
  delete this;
}

void RenderViewDevToolsAgentHost::Observe(int type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDER_VIEW_HOST_DELETED);
  NotifyCloseListener();
}

RenderViewDevToolsAgentHost::~RenderViewDevToolsAgentHost() {
  instances_.erase(render_view_host_);

  int process_id = render_view_host_->process()->id();
  for (Instances::iterator it = instances_.begin();
       it != instances_.end();
       ++it) {
    if (it->first->process()->id() == process_id)
      return;
  }
  // We've disconnected from the last renderer -> revoke cookie permissions.
  ChildProcessSecurityPolicy::GetInstance()->RevokeReadRawCookies(process_id);
}

