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
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_client.h"
#include "content/common/devtools_messages.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return content::GetContentClient()->browser()->GetDevToolsManager();
}

DevToolsManager::DevToolsManager()
    : last_orphan_cookie_(0) {
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
                 NotificationService::AllSources());
}

DevToolsManager::~DevToolsManager() {
  DCHECK(inspected_rvh_to_client_host_.empty());
  DCHECK(client_host_to_inspected_rvh_.empty());
  // By the time we destroy devtools manager, all orphan client hosts should
  // have been delelted, no need to notify them upon tab closing.
  DCHECK(orphan_client_hosts_.empty());
}

DevToolsClientHost* DevToolsManager::GetDevToolsClientHostFor(
    RenderViewHost* inspected_rvh) {
  InspectedRvhToClientHostMap::iterator it =
      inspected_rvh_to_client_host_.find(inspected_rvh);
  if (it != inspected_rvh_to_client_host_.end())
    return it->second;
  return NULL;
}

void DevToolsManager::RegisterDevToolsClientHostFor(
    RenderViewHost* inspected_rvh,
    DevToolsClientHost* client_host) {
  DCHECK(!GetDevToolsClientHostFor(inspected_rvh));

  DevToolsRuntimeProperties initial_properties;
  BindClientHost(inspected_rvh, client_host, initial_properties);
  client_host->set_close_listener(this);
  SendAttachToAgent(inspected_rvh);
}

bool DevToolsManager::ForwardToDevToolsAgent(DevToolsClientHost* from,
                                             const IPC::Message& message) {
  RenderViewHost* inspected_rvh = GetInspectedRenderViewHost(from);
  if (!inspected_rvh)
    return false;

  IPC::Message* m = new IPC::Message(message);
  m->set_routing_id(inspected_rvh->routing_id());
  inspected_rvh->Send(m);
  return true;
}

void DevToolsManager::ForwardToDevToolsClient(RenderViewHost* inspected_rvh,
                                              const IPC::Message& message) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(inspected_rvh);
  if (!client_host) {
    // Client window was closed while there were messages
    // being sent to it.
    return;
  }
  client_host->SendMessageToClient(message);
}

void DevToolsManager::RuntimePropertyChanged(RenderViewHost* inspected_rvh,
                                             const std::string& name,
                                             const std::string& value) {
  RuntimePropertiesMap::iterator it =
      runtime_properties_map_.find(inspected_rvh);
  if (it == runtime_properties_map_.end()) {
    std::pair<RenderViewHost*, DevToolsRuntimeProperties> value(
        inspected_rvh,
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

void DevToolsManager::ClientHostClosing(DevToolsClientHost* host) {
  RenderViewHost* inspected_rvh = GetInspectedRenderViewHost(host);
  if (!inspected_rvh) {
    // It might be in the list of orphan client hosts, remove it from there.
    for (OrphanClientHosts::iterator it = orphan_client_hosts_.begin();
         it != orphan_client_hosts_.end(); ++it) {
      if (it->second.first == host) {
        orphan_client_hosts_.erase(it->first);
        return;
      }
    }
    return;
  }

  NotificationService::current()->Notify(
      content::NOTIFICATION_DEVTOOLS_WINDOW_CLOSING,
      Source<content::BrowserContext>(
          inspected_rvh->site_instance()->GetProcess()->browser_context()),
      Details<RenderViewHost>(inspected_rvh));

  UnbindClientHost(inspected_rvh, host);
}

void DevToolsManager::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDER_VIEW_HOST_DELETED);
  UnregisterDevToolsClientHostFor(Source<RenderViewHost>(source).ptr());
}

RenderViewHost* DevToolsManager::GetInspectedRenderViewHost(
    DevToolsClientHost* client_host) {
  ClientHostToInspectedRvhMap::iterator it =
      client_host_to_inspected_rvh_.find(client_host);
  if (it != client_host_to_inspected_rvh_.end())
    return it->second;
  return NULL;
}

void DevToolsManager::UnregisterDevToolsClientHostFor(
      RenderViewHost* inspected_rvh) {
  DevToolsClientHost* host = GetDevToolsClientHostFor(inspected_rvh);
  if (!host)
    return;
  UnbindClientHost(inspected_rvh, host);
  host->InspectedTabClosing();
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
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(from_rvh);
  if (!client_host)
    return -1;

  int cookie = last_orphan_cookie_++;
  orphan_client_hosts_[cookie] =
      std::pair<DevToolsClientHost*, DevToolsRuntimeProperties>(
          client_host, runtime_properties_map_[from_rvh]);

  UnbindClientHost(from_rvh, client_host);
  return cookie;
}

void DevToolsManager::AttachClientHost(int client_host_cookie,
                                       RenderViewHost* to_rvh) {
  OrphanClientHosts::iterator it = orphan_client_hosts_.find(
      client_host_cookie);
  if (it == orphan_client_hosts_.end())
    return;

  DevToolsClientHost* client_host = (*it).second.first;
  BindClientHost(to_rvh, client_host, (*it).second.second);
  SendAttachToAgent(to_rvh);

  orphan_client_hosts_.erase(client_host_cookie);
}

void DevToolsManager::SendAttachToAgent(RenderViewHost* inspected_rvh) {
  if (inspected_rvh) {
    ChildProcessSecurityPolicy::GetInstance()->GrantReadRawCookies(
        inspected_rvh->process()->id());

    DevToolsRuntimeProperties properties;
    RuntimePropertiesMap::iterator it =
        runtime_properties_map_.find(inspected_rvh);
    if (it != runtime_properties_map_.end()) {
      properties = DevToolsRuntimeProperties(it->second.begin(),
                                            it->second.end());
    }
    inspected_rvh->Send(new DevToolsAgentMsg_Attach(
        inspected_rvh->routing_id(),
        properties));
  }
}

void DevToolsManager::SendDetachToAgent(RenderViewHost* inspected_rvh) {
  if (inspected_rvh) {
    inspected_rvh->Send(new DevToolsAgentMsg_Detach(
        inspected_rvh->routing_id()));
  }
}

void DevToolsManager::BindClientHost(
    RenderViewHost* inspected_rvh,
    DevToolsClientHost* client_host,
    const DevToolsRuntimeProperties& runtime_properties) {
  DCHECK(inspected_rvh_to_client_host_.find(inspected_rvh) ==
      inspected_rvh_to_client_host_.end());
  DCHECK(client_host_to_inspected_rvh_.find(client_host) ==
      client_host_to_inspected_rvh_.end());

  if (client_host_to_inspected_rvh_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableFunction(&DevToolsNetLogObserver::Attach));
  }
  inspected_rvh_to_client_host_[inspected_rvh] = client_host;
  client_host_to_inspected_rvh_[client_host] = inspected_rvh;
  runtime_properties_map_[inspected_rvh] = runtime_properties;
}

void DevToolsManager::UnbindClientHost(RenderViewHost* inspected_rvh,
                                       DevToolsClientHost* client_host) {
  DCHECK(inspected_rvh_to_client_host_.find(inspected_rvh)->second ==
      client_host);
  DCHECK(client_host_to_inspected_rvh_.find(client_host)->second ==
      inspected_rvh);

  inspected_rvh_to_client_host_.erase(inspected_rvh);
  client_host_to_inspected_rvh_.erase(client_host);
  runtime_properties_map_.erase(inspected_rvh);

  if (client_host_to_inspected_rvh_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableFunction(&DevToolsNetLogObserver::Detach));
  }
  SendDetachToAgent(inspected_rvh);

  int process_id = inspected_rvh->process()->id();
  for (InspectedRvhToClientHostMap::iterator it =
           inspected_rvh_to_client_host_.begin();
       it != inspected_rvh_to_client_host_.end();
       ++it) {
    if (it->first->process()->id() == process_id)
      return;
  }
  // We've disconnected from the last renderer -> revoke cookie permissions.
  ChildProcessSecurityPolicy::GetInstance()->RevokeReadRawCookies(process_id);
}

void DevToolsManager::CloseAllClientHosts() {
  std::vector<RenderViewHost*> rhvs;
  for (InspectedRvhToClientHostMap::iterator it =
           inspected_rvh_to_client_host_.begin();
       it != inspected_rvh_to_client_host_.end(); ++it) {
    rhvs.push_back(it->first);
  }
  for (std::vector<RenderViewHost*>::iterator it = rhvs.begin();
       it != rhvs.end(); ++it) {
    UnregisterDevToolsClientHostFor(*it);
  }
}
