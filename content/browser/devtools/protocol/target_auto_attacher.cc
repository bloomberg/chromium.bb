// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/target_auto_attacher.h"

#include "base/containers/queue.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"

namespace content {
namespace protocol {

namespace {

using ScopeAgentsMap =
    std::map<GURL, std::unique_ptr<ServiceWorkerDevToolsAgentHost::List>>;

void GetMatchingHostsByScopeMap(
    const ServiceWorkerDevToolsAgentHost::List& agent_hosts,
    const base::flat_set<GURL>& urls,
    ScopeAgentsMap* scope_agents_map) {
  base::flat_set<base::StringPiece> host_name_set;
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
    const base::flat_set<GURL>& urls) {
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

TargetAutoAttacher::TargetAutoAttacher(AttachCallback attach_callback,
                                       DetachCallback detach_callback)
    : attach_callback_(attach_callback),
      detach_callback_(detach_callback),
      render_frame_host_(nullptr),
      auto_attach_(false),
      wait_for_debugger_on_start_(false),
      attach_to_frames_(false) {}

TargetAutoAttacher::~TargetAutoAttacher() {}

void TargetAutoAttacher::SetRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  render_frame_host_ = render_frame_host;
  UpdateFrames();
  ReattachServiceWorkers(false);
}

void TargetAutoAttacher::UpdateServiceWorkers() {
  ReattachServiceWorkers(false);
}

void TargetAutoAttacher::UpdateFrames() {
  if (!auto_attach_ || !attach_to_frames_)
    return;

  Hosts new_hosts;
  if (render_frame_host_) {
    FrameTreeNode* root = render_frame_host_->frame_tree_node();
    base::queue<FrameTreeNode*> queue;
    queue.push(root);
    while (!queue.empty()) {
      FrameTreeNode* node = queue.front();
      queue.pop();
      bool cross_process = node->current_frame_host()->IsCrossProcessSubframe();
      if (node != root && cross_process) {
        scoped_refptr<DevToolsAgentHost> new_host =
            RenderFrameDevToolsAgentHost::GetOrCreateFor(node);
        new_hosts.insert(new_host);
      } else {
        for (size_t i = 0; i < node->child_count(); ++i)
          queue.push(node->child_at(i));
      }
    }
  }

  // TODO(dgozman): support wait_for_debugger_on_start_.
  ReattachTargetsOfType(new_hosts, DevToolsAgentHost::kTypeFrame, false);
}

void TargetAutoAttacher::AgentHostClosed(DevToolsAgentHost* host) {
  auto_attached_hosts_.erase(base::WrapRefCounted(host));
}

void TargetAutoAttacher::ReattachServiceWorkers(bool waiting_for_debugger) {
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
  Hosts new_hosts;
  for (const auto& pair : matching) {
    if (pair.second->IsReadyForInspection())
      new_hosts.insert(pair.second);
  }
  ReattachTargetsOfType(new_hosts, DevToolsAgentHost::kTypeServiceWorker,
                        waiting_for_debugger);
}

void TargetAutoAttacher::ReattachTargetsOfType(const Hosts& new_hosts,
                                               const std::string& type,
                                               bool waiting_for_debugger) {
  Hosts old_hosts = auto_attached_hosts_;
  for (auto& host : old_hosts) {
    if (host->GetType() == type && new_hosts.find(host) == new_hosts.end()) {
      auto_attached_hosts_.erase(host);
      detach_callback_.Run(host.get());
    }
  }
  for (auto& host : new_hosts) {
    if (old_hosts.find(host) == old_hosts.end()) {
      attach_callback_.Run(host.get(), waiting_for_debugger);
      auto_attached_hosts_.insert(host);
    }
  }
}

void TargetAutoAttacher::SetAutoAttach(bool auto_attach,
                                       bool wait_for_debugger_on_start) {
  wait_for_debugger_on_start_ = wait_for_debugger_on_start;
  if (auto_attach_ == auto_attach)
    return;
  auto_attach_ = auto_attach;
  if (auto_attach_) {
    ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);
    ReattachServiceWorkers(false);
    UpdateFrames();
  } else {
    ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
    Hosts empty;
    ReattachTargetsOfType(empty, DevToolsAgentHost::kTypeFrame, false);
    ReattachTargetsOfType(empty, DevToolsAgentHost::kTypeServiceWorker, false);
    DCHECK(auto_attached_hosts_.empty());
  }
}

void TargetAutoAttacher::SetAttachToFrames(bool attach_to_frames) {
  if (attach_to_frames_ == attach_to_frames)
    return;
  attach_to_frames_ = attach_to_frames;
  if (attach_to_frames_) {
    UpdateFrames();
  } else {
    Hosts empty;
    ReattachTargetsOfType(empty, DevToolsAgentHost::kTypeFrame, false);
  }
}

// -------- ServiceWorkerDevToolsManager::Observer ----------

void TargetAutoAttacher::WorkerCreated(ServiceWorkerDevToolsAgentHost* host) {
  BrowserContext* browser_context = nullptr;
  if (render_frame_host_)
    browser_context = render_frame_host_->GetProcess()->GetBrowserContext();
  auto hosts = GetMatchingServiceWorkers(browser_context, frame_urls_);
  if (hosts.find(host->GetId()) != hosts.end() && !host->IsAttached() &&
      !host->IsPausedForDebugOnStart() && wait_for_debugger_on_start_) {
    host->PauseForDebugOnStart();
  }
}

void TargetAutoAttacher::WorkerReadyForInspection(
    ServiceWorkerDevToolsAgentHost* host) {
  DCHECK(host->IsReadyForInspection());
  if (ServiceWorkerDevToolsManager::GetInstance()
          ->debug_service_worker_on_start()) {
    // When debug_service_worker_on_start is true, a new DevTools window will
    // be opened in ServiceWorkerDevToolsManager::WorkerReadyForInspection.
    return;
  }
  ReattachServiceWorkers(host->IsPausedForDebugOnStart());
}

void TargetAutoAttacher::WorkerVersionInstalled(
    ServiceWorkerDevToolsAgentHost* host) {
  ReattachServiceWorkers(false);
}

void TargetAutoAttacher::WorkerVersionDoomed(
    ServiceWorkerDevToolsAgentHost* host) {
  ReattachServiceWorkers(false);
}

void TargetAutoAttacher::WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) {
  ReattachServiceWorkers(false);
}

}  // namespace protocol
}  // namespace content
