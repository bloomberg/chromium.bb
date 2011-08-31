// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_manager.h"

#include <vector>

#include "base/message_loop.h"
#include "content/browser/browser_thread.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/debugger/devtools_client_host.h"
#include "content/browser/debugger/devtools_netlog_observer.h"
#include "content/browser/debugger/render_view_devtools_agent_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_client.h"
#include "content/common/devtools_messages.h"
#include "googleurl/src/gurl.h"

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return content::GetContentClient()->browser()->GetDevToolsManager();
}

DevToolsManager::DevToolsManager()
    : last_orphan_cookie_(0) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(agent_to_client_host_.empty());
  DCHECK(client_to_agent_host_.empty());
  // By the time we destroy devtools manager, all orphan client hosts should
  // have been delelted, no need to notify them upon tab closing.
  DCHECK(orphan_client_hosts_.empty());
}

DevToolsClientHost* DevToolsManager::GetDevToolsClientHostFor(
    RenderViewHost* inspected_rvh) {
  DevToolsAgentHost* agent_host = RenderViewDevToolsAgentHost::FindFor(
      inspected_rvh);
  return GetDevToolsClientHostFor(agent_host);
}

DevToolsClientHost* DevToolsManager::GetDevToolsClientHostFor(
    DevToolsAgentHost* agent_host) {
  AgentToClientHostMap::iterator it = agent_to_client_host_.find(agent_host);
  if (it != agent_to_client_host_.end())
    return it->second;
  return NULL;
}

void DevToolsManager::RegisterDevToolsClientHostFor(
    RenderViewHost* inspected_rvh,
    DevToolsClientHost* client_host) {
  DCHECK(!GetDevToolsClientHostFor(inspected_rvh));

  DevToolsRuntimeProperties initial_properties;
  DevToolsAgentHost* agent_host = RenderViewDevToolsAgentHost::FindFor(
      inspected_rvh);
  BindClientHost(agent_host, client_host, initial_properties);
  client_host->set_close_listener(this);
  SendAttachToAgent(agent_host);
}

bool DevToolsManager::ForwardToDevToolsAgent(DevToolsClientHost* from,
                                             const IPC::Message& message) {
  DevToolsAgentHost* agent_host = GetAgentHost(from);
  if (!agent_host)
    return false;

  agent_host->SendMessageToAgent(new IPC::Message(message));
  return true;
}

void DevToolsManager::ForwardToDevToolsClient(DevToolsAgentHost* agent_host,
                                              const IPC::Message& message) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(agent_host);
  if (!client_host) {
    // Client window was closed while there were messages
    // being sent to it.
    return;
  }
  client_host->SendMessageToClient(message);
}

void DevToolsManager::RuntimePropertyChanged(DevToolsAgentHost* agent_host,
                                             const std::string& name,
                                             const std::string& value) {
  RuntimePropertiesMap::iterator it =
      runtime_properties_map_.find(agent_host);
  if (it == runtime_properties_map_.end()) {
    std::pair<DevToolsAgentHost*, DevToolsRuntimeProperties> value(
        agent_host,
        DevToolsRuntimeProperties());
    it = runtime_properties_map_.insert(value).first;
  }
  it->second[name] = value;
}

void DevToolsManager::SendInspectElement(RenderViewHost* inspected_rvh,
                                         int x,
                                         int y) {
  inspected_rvh->Send(new DevToolsAgentMsg_InspectElement(
      inspected_rvh->routing_id(),
      x,
      y));
}

void DevToolsManager::ClientHostClosing(DevToolsClientHost* client_host) {
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

void DevToolsManager::AgentHostClosing(DevToolsAgentHost* agent_host) {
  UnregisterDevToolsClientHostFor(agent_host);
}

DevToolsAgentHost* DevToolsManager::GetAgentHost(
    DevToolsClientHost* client_host) {
  ClientHostToInspectedRvhMap::iterator it =
      client_to_agent_host_.find(client_host);
  if (it != client_to_agent_host_.end())
    return it->second;
  return NULL;
}

void DevToolsManager::UnregisterDevToolsClientHostFor(
      RenderViewHost* inspected_rvh) {
  DevToolsAgentHost* agent_host = RenderViewDevToolsAgentHost::FindFor(
      inspected_rvh);
  UnregisterDevToolsClientHostFor(agent_host);
}

void DevToolsManager::UnregisterDevToolsClientHostFor(
    DevToolsAgentHost* agent_host) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(agent_host);
  if (!client_host)
    return;
  UnbindClientHost(agent_host, client_host);
  client_host->InspectedTabClosing();
}

void DevToolsManager::OnNavigatingToPendingEntry(RenderViewHost* rvh,
                                                 RenderViewHost* dest_rvh,
                                                 const GURL& gurl) {
  int cookie = DetachClientHost(rvh);
  if (cookie != -1) {
    // Navigating to URL in the inspected window.
    AttachClientHost(cookie, dest_rvh);

    DevToolsClientHost* client_host = GetDevToolsClientHostFor(dest_rvh);
    client_host->FrameNavigating(gurl.spec());
  }
}

void DevToolsManager::TabReplaced(TabContents* old_tab,
                                  TabContents* new_tab) {
  RenderViewHost* old_rvh = old_tab->render_view_host();
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(old_rvh);
  if (!client_host)
    return;  // Didn't know about old_tab.
  int cookie = DetachClientHost(old_rvh);
  if (cookie == -1)
    return;  // Didn't know about old_tab.

  client_host->TabReplaced(new_tab);
  AttachClientHost(cookie, new_tab->render_view_host());
}

int DevToolsManager::DetachClientHost(RenderViewHost* from_rvh) {
  DevToolsAgentHost* agent_host = RenderViewDevToolsAgentHost::FindFor(
      from_rvh);
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(agent_host);
  if (!client_host)
    return -1;

  int cookie = last_orphan_cookie_++;
  orphan_client_hosts_[cookie] =
      std::pair<DevToolsClientHost*, DevToolsRuntimeProperties>(
          client_host, runtime_properties_map_[agent_host]);

  UnbindClientHost(agent_host, client_host);
  return cookie;
}

void DevToolsManager::AttachClientHost(int client_host_cookie,
                                       RenderViewHost* to_rvh) {
  OrphanClientHosts::iterator it = orphan_client_hosts_.find(
      client_host_cookie);
  if (it == orphan_client_hosts_.end())
    return;

  DevToolsClientHost* client_host = (*it).second.first;
  DevToolsAgentHost* agent_host = RenderViewDevToolsAgentHost::FindFor(
      to_rvh);
  BindClientHost(agent_host, client_host, (*it).second.second);
  SendAttachToAgent(agent_host);

  orphan_client_hosts_.erase(client_host_cookie);
}

void DevToolsManager::SendAttachToAgent(DevToolsAgentHost* agent_host) {
  DevToolsRuntimeProperties properties;
  RuntimePropertiesMap::iterator it =
      runtime_properties_map_.find(agent_host);
  if (it != runtime_properties_map_.end()) {
    properties = DevToolsRuntimeProperties(it->second.begin(),
                                           it->second.end());
  }
  agent_host->SendMessageToAgent(new DevToolsAgentMsg_Attach(
      MSG_ROUTING_NONE,
      properties));
}

void DevToolsManager::SendDetachToAgent(DevToolsAgentHost* agent_host) {
  agent_host->SendMessageToAgent(new DevToolsAgentMsg_Detach(
      MSG_ROUTING_NONE));
}

void DevToolsManager::BindClientHost(
    DevToolsAgentHost* agent_host,
    DevToolsClientHost* client_host,
    const DevToolsRuntimeProperties& runtime_properties) {
  DCHECK(agent_to_client_host_.find(agent_host) ==
      agent_to_client_host_.end());
  DCHECK(client_to_agent_host_.find(client_host) ==
      client_to_agent_host_.end());

  if (client_to_agent_host_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableFunction(&DevToolsNetLogObserver::Attach));
  }
  agent_to_client_host_[agent_host] = client_host;
  client_to_agent_host_[client_host] = agent_host;
  runtime_properties_map_[agent_host] = runtime_properties;
  agent_host->set_close_listener(this);

  int process_id = agent_host->GetRenderProcessId();
  if (process_id != -1)
    ChildProcessSecurityPolicy::GetInstance()->GrantReadRawCookies(process_id);
}

void DevToolsManager::UnbindClientHost(DevToolsAgentHost* agent_host,
                                       DevToolsClientHost* client_host) {
  DCHECK(agent_host);
  DCHECK(agent_to_client_host_.find(agent_host)->second ==
      client_host);
  DCHECK(client_to_agent_host_.find(client_host)->second ==
      agent_host);
  agent_host->set_close_listener(NULL);

  agent_to_client_host_.erase(agent_host);
  client_to_agent_host_.erase(client_host);
  runtime_properties_map_.erase(agent_host);

  if (client_to_agent_host_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableFunction(&DevToolsNetLogObserver::Detach));
  }
  SendDetachToAgent(agent_host);

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

void DevToolsManager::CloseAllClientHosts() {
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
