// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/target_handler.h"

#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"

namespace content {
namespace devtools {
namespace target {

using Response = DevToolsProtocolClient::Response;

namespace {

using ScopeAgentsMap =
    std::map<GURL, std::unique_ptr<ServiceWorkerDevToolsAgentHost::List>>;

void GetMatchingHostsByScopeMap(
    const ServiceWorkerDevToolsAgentHost::List& agent_hosts,
    const std::set<GURL>& urls,
    ScopeAgentsMap* scope_agents_map) {
  std::set<base::StringPiece> host_name_set;
  for (const GURL& url : urls)
    host_name_set.insert(url.host_piece());
  for (const auto& host : agent_hosts) {
    if (host_name_set.find(host->scope().host_piece()) == host_name_set.end())
      continue;
    const auto& it = scope_agents_map->find(host->scope());
    if (it == scope_agents_map->end()) {
      std::unique_ptr<ServiceWorkerDevToolsAgentHost::List> new_list(
          new ServiceWorkerDevToolsAgentHost::List());
      new_list->push_back(host);
      (*scope_agents_map)[host->scope()] = std::move(new_list);
    } else {
      it->second->push_back(host);
    }
  }
}

void AddEligibleHosts(const ServiceWorkerDevToolsAgentHost::List& list,
                      ServiceWorkerDevToolsAgentHost::Map* result) {
  base::Time last_installed_time;
  base::Time last_doomed_time;
  for (const auto& host : list) {
    if (host->version_installed_time() > last_installed_time)
      last_installed_time = host->version_installed_time();
    if (host->version_doomed_time() > last_doomed_time)
      last_doomed_time = host->version_doomed_time();
  }
  for (const auto& host : list) {
    // We don't attech old redundant Service Workers when there is newer
    // installed Service Worker.
    if (host->version_doomed_time().is_null() ||
        (last_installed_time < last_doomed_time &&
         last_doomed_time == host->version_doomed_time())) {
      (*result)[host->GetId()] = host;
    }
  }
}

ServiceWorkerDevToolsAgentHost::Map GetMatchingServiceWorkers(
    BrowserContext* browser_context,
    const std::set<GURL>& urls) {
  ServiceWorkerDevToolsAgentHost::Map result;
  if (!browser_context)
    return result;

  ServiceWorkerDevToolsAgentHost::List agent_hosts;
  ServiceWorkerDevToolsManager::GetInstance()
      ->AddAllAgentHostsForBrowserContext(browser_context, &agent_hosts);

  ScopeAgentsMap scope_agents_map;
  GetMatchingHostsByScopeMap(agent_hosts, urls, &scope_agents_map);

  for (const auto& it : scope_agents_map)
    AddEligibleHosts(*it.second.get(), &result);

  return result;
}

}  // namespace

TargetHandler::TargetHandler()
    : enabled_(false),
      wait_for_debugger_on_start_(false),
      render_frame_host_(nullptr) {
}

TargetHandler::~TargetHandler() {
  Disable();
}

void TargetHandler::SetRenderFrameHost(RenderFrameHostImpl* render_frame_host) {
  render_frame_host_ = render_frame_host;
}

void TargetHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

void TargetHandler::Detached() {
  Disable();
}

void TargetHandler::UpdateServiceWorkers() {
  UpdateServiceWorkers(false);
}

void TargetHandler::UpdateServiceWorkers(bool waiting_for_debugger) {
  if (!enabled_)
    return;

  frame_urls_.clear();
  BrowserContext* browser_context = nullptr;
  if (render_frame_host_) {
    // TODO(dgozman): do not traverse inside cross-process subframes.
    for (FrameTreeNode* node :
         render_frame_host_->frame_tree_node()->frame_tree()->Nodes()) {
      frame_urls_.insert(node->current_url());
    }
    browser_context = render_frame_host_->GetProcess()->GetBrowserContext();
  }

  std::map<std::string, scoped_refptr<DevToolsAgentHost>> old_hosts =
      attached_hosts_;
  ServiceWorkerDevToolsAgentHost::Map new_hosts =
      GetMatchingServiceWorkers(browser_context, frame_urls_);

  for (const auto& pair : old_hosts) {
    if (pair.second->GetType() == DevToolsAgentHost::kTypeServiceWorker &&
        new_hosts.find(pair.first) == new_hosts.end()) {
      DetachFromTargetInternal(pair.second.get());
    }
  }

  for (const auto& pair : new_hosts) {
    if (old_hosts.find(pair.first) == old_hosts.end())
      AttachToTargetInternal(pair.second.get(), waiting_for_debugger);
  }
}

void TargetHandler::AttachToTargetInternal(
    DevToolsAgentHost* host, bool waiting_for_debugger) {
  if (host->IsAttached())
    return;
  attached_hosts_[host->GetId()] = host;
  host->AttachClient(this);
  client_->TargetCreated(TargetCreatedParams::Create()
      ->set_target_id(host->GetId())
      ->set_url(host->GetURL().spec())
      ->set_type(host->GetType())
      ->set_waiting_for_debugger(waiting_for_debugger));
}

void TargetHandler::DetachFromTargetInternal(DevToolsAgentHost* host) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;
  host->DetachClient(this);
  client_->TargetRemoved(TargetRemovedParams::Create()->
      set_target_id(host->GetId()));
  attached_hosts_.erase(it);
}

// ----------------- Protocol ----------------------

Response TargetHandler::Enable() {
  if (enabled_)
    return Response::OK();
  enabled_ = true;
  ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);
  UpdateServiceWorkers();
  return Response::OK();
}

Response TargetHandler::Disable() {
  if (!enabled_)
    return Response::OK();
  enabled_ = false;
  wait_for_debugger_on_start_ = false;
  ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
  for (const auto& pair : attached_hosts_)
    pair.second->DetachClient(this);
  attached_hosts_.clear();
  return Response::OK();
}

Response TargetHandler::SetWaitForDebuggerOnStart(bool value) {
  wait_for_debugger_on_start_ = value;
  return Response::OK();
}

Response TargetHandler::SendMessageToTarget(
    const std::string& target_id,
    const std::string& message) {
  auto it = attached_hosts_.find(target_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not attached to the target");
  it->second->DispatchProtocolMessage(this, message);
  return Response::OK();
}

Response TargetHandler::GetTargetInfo(
    const std::string& target_id,
    scoped_refptr<TargetInfo>* target_info) {
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("No target with such id");
  *target_info =TargetInfo::Create()
      ->set_target_id(agent_host->GetId())
      ->set_type(agent_host->GetType())
      ->set_title(agent_host->GetTitle())
      ->set_url(agent_host->GetURL().spec());
  return Response::OK();
}

Response TargetHandler::ActivateTarget(const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("No target with such id");
  agent_host->Activate();
  return Response::OK();
}

// ---------------- DevToolsAgentHostClient ----------------

void TargetHandler::DispatchProtocolMessage(
    DevToolsAgentHost* host,
    const std::string& message) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;  // Already disconnected.

  client_->ReceivedMessageFromTarget(
      ReceivedMessageFromTargetParams::Create()->
          set_target_id(host->GetId())->
          set_message(message));
}

void TargetHandler::AgentHostClosed(
    DevToolsAgentHost* host,
    bool replaced_with_another_client) {
  client_->TargetRemoved(TargetRemovedParams::Create()->
      set_target_id(host->GetId()));
  attached_hosts_.erase(host->GetId());
}

// -------- ServiceWorkerDevToolsManager::Observer ----------

void TargetHandler::WorkerCreated(
    ServiceWorkerDevToolsAgentHost* host) {
  BrowserContext* browser_context = nullptr;
  if (render_frame_host_)
    browser_context = render_frame_host_->GetProcess()->GetBrowserContext();
  auto hosts = GetMatchingServiceWorkers(browser_context, frame_urls_);
  if (hosts.find(host->GetId()) != hosts.end() && !host->IsAttached() &&
      !host->IsPausedForDebugOnStart() && wait_for_debugger_on_start_) {
    host->PauseForDebugOnStart();
  }
}

void TargetHandler::WorkerReadyForInspection(
    ServiceWorkerDevToolsAgentHost* host) {
  if (ServiceWorkerDevToolsManager::GetInstance()
          ->debug_service_worker_on_start()) {
    // When debug_service_worker_on_start is true, a new DevTools window will
    // be opened in ServiceWorkerDevToolsManager::WorkerReadyForInspection.
    return;
  }
  UpdateServiceWorkers(host->IsPausedForDebugOnStart());
}

void TargetHandler::WorkerVersionInstalled(
    ServiceWorkerDevToolsAgentHost* host) {
  UpdateServiceWorkers();
}

void TargetHandler::WorkerVersionDoomed(
    ServiceWorkerDevToolsAgentHost* host) {
  UpdateServiceWorkers();
}

void TargetHandler::WorkerDestroyed(
    ServiceWorkerDevToolsAgentHost* host) {
  UpdateServiceWorkers();
}

}  // namespace target
}  // namespace devtools
}  // namespace content
