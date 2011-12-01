// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_manager_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/debugger/devtools_netlog_observer.h"
#include "content/browser/debugger/render_view_devtools_agent_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "googleurl/src/gurl.h"

namespace content {

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return DevToolsManagerImpl::GetInstance();
}

// static
DevToolsManagerImpl* DevToolsManagerImpl::GetInstance() {
  return Singleton<DevToolsManagerImpl>::get();
}

DevToolsManagerImpl::DevToolsManagerImpl()
    : last_orphan_cookie_(0) {
}

DevToolsManagerImpl::~DevToolsManagerImpl() {
  DCHECK(agent_to_client_host_.empty());
  DCHECK(client_to_agent_host_.empty());
  // By the time we destroy devtools manager, all orphan client hosts should
  // have been delelted, no need to notify them upon tab closing.
  DCHECK(orphan_client_hosts_.empty());
}

DevToolsClientHost* DevToolsManagerImpl::GetDevToolsClientHostFor(
    DevToolsAgentHost* agent_host) {
  AgentToClientHostMap::iterator it = agent_to_client_host_.find(agent_host);
  if (it != agent_to_client_host_.end())
    return it->second;
  return NULL;
}

void DevToolsManagerImpl::RegisterDevToolsClientHostFor(
    DevToolsAgentHost* agent_host,
    DevToolsClientHost* client_host) {
  BindClientHost(agent_host, client_host);
  agent_host->Attach();
}

bool DevToolsManagerImpl::DispatchOnInspectorBackend(
    DevToolsClientHost* from,
    const std::string& message) {
  DevToolsAgentHost* agent_host = GetAgentHost(from);
  if (!agent_host)
    return false;

  agent_host->DipatchOnInspectorBackend(message);
  return true;
}

void DevToolsManagerImpl::DispatchOnInspectorFrontend(
    DevToolsAgentHost* agent_host,
    const std::string& message) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(agent_host);
  if (!client_host) {
    // Client window was closed while there were messages
    // being sent to it.
    return;
  }
  client_host->DispatchOnInspectorFrontend(message);
}

void DevToolsManagerImpl::SaveAgentRuntimeState(DevToolsAgentHost* agent_host,
                                                const std::string& state) {
  agent_runtime_states_[agent_host] = state;
}

void DevToolsManagerImpl::InspectElement(DevToolsAgentHost* agent_host,
                                         int x, int y) {
  agent_host->InspectElement(x, y);
}

void DevToolsManagerImpl::ClientHostClosing(DevToolsClientHost* client_host) {
  DevToolsAgentHost* agent_host = GetAgentHost(client_host);
  if (!agent_host) {
    // It might be in the list of orphan client hosts, remove it from there.
    for (OrphanClientHosts::iterator it = orphan_client_hosts_.begin();
         it != orphan_client_hosts_.end(); ++it) {
      if (it->second.first == client_host) {
        orphan_client_hosts_.erase(it->first);
        return;
      }
    }
    return;
  }

  agent_host->NotifyClientClosing();

  UnbindClientHost(agent_host, client_host);
}

void DevToolsManagerImpl::AgentHostClosing(DevToolsAgentHost* agent_host) {
  UnregisterDevToolsClientHostFor(agent_host);
}

DevToolsAgentHost* DevToolsManagerImpl::GetAgentHost(
    DevToolsClientHost* client_host) {
  ClientHostToInspectedRvhMap::iterator it =
      client_to_agent_host_.find(client_host);
  if (it != client_to_agent_host_.end())
    return it->second;
  return NULL;
}

void DevToolsManagerImpl::UnregisterDevToolsClientHostFor(
    DevToolsAgentHost* agent_host) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(agent_host);
  if (!client_host)
    return;
  UnbindClientHost(agent_host, client_host);
  client_host->InspectedTabClosing();
}

void DevToolsManagerImpl::OnNavigatingToPendingEntry(RenderViewHost* rvh,
                                                     RenderViewHost* dest_rvh,
                                                     const GURL& gurl) {
  if (rvh == dest_rvh && rvh->render_view_termination_status() ==
          base::TERMINATION_STATUS_STILL_RUNNING)
    return;
  int cookie = DetachClientHost(rvh);
  if (cookie != -1) {
    // Navigating to URL in the inspected window.
    AttachClientHost(cookie, dest_rvh);

    DevToolsAgentHost* dest_agent_host =
        DevToolsAgentHostRegistry::GetDevToolsAgentHost(dest_rvh);
    DevToolsClientHost* client_host = GetDevToolsClientHostFor(
        dest_agent_host);
    client_host->FrameNavigating(gurl.spec());
  }
}

void DevToolsManagerImpl::OnCancelPendingNavigation(RenderViewHost* pending,
                                                    RenderViewHost* current) {
  int cookie = DetachClientHost(pending);
  if (cookie != -1) {
    // Navigating to URL in the inspected window.
    AttachClientHost(cookie, current);
  }
}

void DevToolsManagerImpl::TabReplaced(TabContents* old_tab,
                                      TabContents* new_tab) {
  RenderViewHost* old_rvh = old_tab->render_view_host();
  if (!DevToolsAgentHostRegistry::HasDevToolsAgentHost(old_rvh))
    return;

  DevToolsAgentHost* old_agent_host =
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(old_rvh);
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(old_agent_host);
  if (!client_host)
    return;  // Didn't know about old_tab.
  int cookie = DetachClientHost(old_rvh);
  if (cookie == -1)
    return;  // Didn't know about old_tab.

  client_host->TabReplaced(new_tab);
  AttachClientHost(cookie, new_tab->render_view_host());
}

int DevToolsManagerImpl::DetachClientHost(RenderViewHost* from_rvh) {
  DevToolsAgentHost* agent_host =
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(from_rvh);
  return DetachClientHost(agent_host);
}

int DevToolsManagerImpl::DetachClientHost(DevToolsAgentHost* agent_host) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(agent_host);
  if (!client_host)
    return -1;

  int cookie = last_orphan_cookie_++;
  orphan_client_hosts_[cookie] =
      std::pair<DevToolsClientHost*, std::string>(
          client_host, agent_runtime_states_[agent_host]);

  UnbindClientHost(agent_host, client_host);
  return cookie;
}

void DevToolsManagerImpl::AttachClientHost(int client_host_cookie,
                                           RenderViewHost* to_rvh) {
  DevToolsAgentHost* agent_host =
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(to_rvh);
  AttachClientHost(client_host_cookie, agent_host);
}

void DevToolsManagerImpl::AttachClientHost(int client_host_cookie,
                                           DevToolsAgentHost* agent_host) {
  OrphanClientHosts::iterator it = orphan_client_hosts_.find(
      client_host_cookie);
  if (it == orphan_client_hosts_.end())
    return;

  DevToolsClientHost* client_host = (*it).second.first;
  const std::string& state = (*it).second.second;
  BindClientHost(agent_host, client_host);
  agent_host->Reattach(state);
  agent_runtime_states_[agent_host] = state;

  orphan_client_hosts_.erase(it);
}

void DevToolsManagerImpl::BindClientHost(
    DevToolsAgentHost* agent_host,
    DevToolsClientHost* client_host) {
  DCHECK(agent_to_client_host_.find(agent_host) ==
      agent_to_client_host_.end());
  DCHECK(client_to_agent_host_.find(client_host) ==
      client_to_agent_host_.end());

  if (client_to_agent_host_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DevToolsNetLogObserver::Attach));
  }
  agent_to_client_host_[agent_host] = client_host;
  client_to_agent_host_[client_host] = agent_host;
  agent_host->set_close_listener(this);

  int process_id = agent_host->GetRenderProcessId();
  if (process_id != -1)
    ChildProcessSecurityPolicy::GetInstance()->GrantReadRawCookies(process_id);
}

void DevToolsManagerImpl::UnbindClientHost(DevToolsAgentHost* agent_host,
                                           DevToolsClientHost* client_host) {
  DCHECK(agent_host);
  DCHECK(agent_to_client_host_.find(agent_host)->second ==
      client_host);
  DCHECK(client_to_agent_host_.find(client_host)->second ==
      agent_host);
  agent_host->set_close_listener(NULL);

  agent_to_client_host_.erase(agent_host);
  client_to_agent_host_.erase(client_host);
  agent_runtime_states_.erase(agent_host);

  if (client_to_agent_host_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DevToolsNetLogObserver::Detach));
  }
  agent_host->Detach();

  int process_id = agent_host->GetRenderProcessId();
  if (process_id == -1)
    return;
  for (AgentToClientHostMap::iterator it = agent_to_client_host_.begin();
       it != agent_to_client_host_.end();
       ++it) {
    if (it->first->GetRenderProcessId() == process_id)
      return;
  }
  // We've disconnected from the last renderer -> revoke cookie permissions.
  ChildProcessSecurityPolicy::GetInstance()->RevokeReadRawCookies(process_id);
}

void DevToolsManagerImpl::CloseAllClientHosts() {
  std::vector<DevToolsAgentHost*> agents;
  for (AgentToClientHostMap::iterator it =
           agent_to_client_host_.begin();
       it != agent_to_client_host_.end(); ++it) {
    agents.push_back(it->first);
  }
  for (std::vector<DevToolsAgentHost*>::iterator it = agents.begin();
       it != agents.end(); ++it) {
    UnregisterDevToolsClientHostFor(*it);
  }
}

}  // namespace content
