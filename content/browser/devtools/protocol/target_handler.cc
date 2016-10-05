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
    : discover_(false),
      auto_attach_(false),
      wait_for_debugger_on_start_(false),
      attach_to_frames_(false),
      render_frame_host_(nullptr) {
}

TargetHandler::~TargetHandler() {
  Detached();
}

void TargetHandler::SetRenderFrameHost(RenderFrameHostImpl* render_frame_host) {
  render_frame_host_ = render_frame_host;
  UpdateFrames();
}

void TargetHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

void TargetHandler::Detached() {
  SetAutoAttach(false, false);
  SetDiscoverTargets(false);
}

void TargetHandler::UpdateServiceWorkers() {
  UpdateServiceWorkers(false);
}

void TargetHandler::UpdateFrames() {
  if (!auto_attach_ || !attach_to_frames_)
    return;

  HostsMap new_hosts;
  if (render_frame_host_) {
    FrameTreeNode* root = render_frame_host_->frame_tree_node();
    std::queue<FrameTreeNode*> queue;
    queue.push(root);
    while (!queue.empty()) {
      FrameTreeNode* node = queue.front();
      queue.pop();
      bool cross_process = node->current_frame_host()->IsCrossProcessSubframe();
      if (node != root && cross_process) {
        scoped_refptr<DevToolsAgentHost> new_host =
            DevToolsAgentHost::GetOrCreateFor(node->current_frame_host());
        new_hosts[new_host->GetId()] = new_host;
      } else {
        for (size_t i = 0; i < node->child_count(); ++i)
          queue.push(node->child_at(i));
      }
    }
  }

  // TODO(dgozman): support wait_for_debugger_on_start_.
  ReattachTargetsOfType(new_hosts, DevToolsAgentHost::kTypeFrame, false);
}

void TargetHandler::UpdateServiceWorkers(bool waiting_for_debugger) {
  if (!auto_attach_)
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

  auto matching = GetMatchingServiceWorkers(browser_context, frame_urls_);
  HostsMap new_hosts;
  for (const auto& pair : matching)
    new_hosts[pair.first] = pair.second;
  ReattachTargetsOfType(
      new_hosts, DevToolsAgentHost::kTypeServiceWorker, waiting_for_debugger);
}

void TargetHandler::ReattachTargetsOfType(
    const HostsMap& new_hosts,
    const std::string& type,
    bool waiting_for_debugger) {
  HostsMap old_hosts = attached_hosts_;
  for (const auto& pair : old_hosts) {
    if (pair.second->GetType() == type &&
        new_hosts.find(pair.first) == new_hosts.end()) {
      DetachFromTargetInternal(pair.second.get());
      TargetRemovedInternal(pair.second.get());
    }
  }
  for (const auto& pair : new_hosts) {
    if (old_hosts.find(pair.first) == old_hosts.end()) {
      TargetCreatedInternal(pair.second.get());
      AttachToTargetInternal(pair.second.get(), waiting_for_debugger);
    }
  }
}

void TargetHandler::TargetCreatedInternal(DevToolsAgentHost* host) {
  client_->TargetCreated(
      TargetCreatedParams::Create()->set_target_info(
          TargetInfo::Create()->set_target_id(host->GetId())
                              ->set_title(host->GetTitle())
                              ->set_url(host->GetURL().spec())
                              ->set_type(host->GetType())));
}

void TargetHandler::TargetRemovedInternal(DevToolsAgentHost* host) {
  client_->TargetRemoved(TargetRemovedParams::Create()
      ->set_target_id(host->GetId()));
}

bool TargetHandler::AttachToTargetInternal(
    DevToolsAgentHost* host, bool waiting_for_debugger) {
  if (!host->AttachClient(this))
    return false;
  attached_hosts_[host->GetId()] = host;
  client_->AttachedToTarget(AttachedToTargetParams::Create()
      ->set_target_id(host->GetId())
      ->set_waiting_for_debugger(waiting_for_debugger));
  return true;
}

void TargetHandler::DetachFromTargetInternal(DevToolsAgentHost* host) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;
  host->DetachClient(this);
  client_->DetachedFromTarget(DetachedFromTargetParams::Create()->
      set_target_id(host->GetId()));
  attached_hosts_.erase(it);
}

// ----------------- Protocol ----------------------

Response TargetHandler::SetDiscoverTargets(bool discover) {
  if (discover_ == discover)
    return Response::OK();
  discover_ = discover;
  // TODO(dgozman): observe all agent hosts here.
  return Response::OK();
}

Response TargetHandler::SetAutoAttach(
    bool auto_attach, bool wait_for_debugger_on_start) {
  wait_for_debugger_on_start_ = wait_for_debugger_on_start;
  if (auto_attach_ == auto_attach)
    return Response::OK();
  auto_attach_ = auto_attach;
  if (auto_attach_) {
    ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);
    UpdateServiceWorkers();
    UpdateFrames();
  } else {
    ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
    HostsMap empty;
    ReattachTargetsOfType(empty, DevToolsAgentHost::kTypeFrame, false);
    ReattachTargetsOfType(empty, DevToolsAgentHost::kTypeServiceWorker, false);
  }
  return Response::OK();
}

Response TargetHandler::SetAttachToFrames(bool value) {
  if (attach_to_frames_ == value)
    return Response::OK();
  attach_to_frames_ = value;
  if (attach_to_frames_) {
    UpdateFrames();
  } else {
    HostsMap empty;
    ReattachTargetsOfType(empty, DevToolsAgentHost::kTypeFrame, false);
  }
  return Response::OK();
}

Response TargetHandler::AttachToTarget(const std::string& target_id,
                                       bool* out_success) {
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("No target with such id");
  *out_success = AttachToTargetInternal(agent_host.get(), false);
  return Response::OK();
}

Response TargetHandler::DetachFromTarget(const std::string& target_id) {
  auto it = attached_hosts_.find(target_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not attached to the target");
  DetachFromTargetInternal(it->second.get());
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
  *target_info = TargetInfo::Create()
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
  client_->DetachedFromTarget(DetachedFromTargetParams::Create()->
      set_target_id(host->GetId()));
  attached_hosts_.erase(host->GetId());
  TargetRemovedInternal(host);
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
