// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_manager.h"

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_listener.h"

namespace content {

ServiceWorkerDevToolsManager::ServiceWorkerIdentifier::ServiceWorkerIdentifier(
    const ServiceWorkerContextCore* context,
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id,
    const GURL& url,
    const GURL& scope,
    const base::UnguessableToken& devtools_worker_token)
    : context_(context),
      context_weak_(context_weak),
      version_id_(version_id),
      url_(url),
      scope_(scope),
      devtools_worker_token_(devtools_worker_token) {
  DCHECK(!devtools_worker_token_.is_empty());
}

ServiceWorkerDevToolsManager::ServiceWorkerIdentifier::ServiceWorkerIdentifier(
    const ServiceWorkerIdentifier& other) = default;

ServiceWorkerDevToolsManager::
ServiceWorkerIdentifier::~ServiceWorkerIdentifier() {
}

bool ServiceWorkerDevToolsManager::ServiceWorkerIdentifier::Matches(
    const ServiceWorkerIdentifier& other) const {
  return context_ == other.context_ && version_id_ == other.version_id_;
}

// static
ServiceWorkerDevToolsManager* ServiceWorkerDevToolsManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<ServiceWorkerDevToolsManager>::get();
}

ServiceWorkerDevToolsAgentHost*
ServiceWorkerDevToolsManager::GetDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  AgentHostMap::iterator it = workers_.find(
      WorkerId(worker_process_id, worker_route_id));
  return it == workers_.end() ? NULL : it->second;
}

void ServiceWorkerDevToolsManager::AddAllAgentHosts(
    ServiceWorkerDevToolsAgentHost::List* result) {
  for (auto& worker : workers_) {
    if (!worker.second->IsTerminated())
      result->push_back(worker.second);
  }
}

void ServiceWorkerDevToolsManager::AddAllAgentHostsForBrowserContext(
    BrowserContext* browser_context,
    ServiceWorkerDevToolsAgentHost::List* result) {
  for (auto& worker : workers_) {
    if (!worker.second->IsTerminated() &&
        worker.second->GetBrowserContext() == browser_context) {
      result->push_back(worker.second);
    }
  }
}

bool ServiceWorkerDevToolsManager::WorkerCreated(
    int worker_process_id,
    int worker_route_id,
    const ServiceWorkerIdentifier& service_worker_id,
    bool is_installed_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = FindExistingWorkerAgentHost(service_worker_id);
  if (it == workers_.end()) {
    scoped_refptr<ServiceWorkerDevToolsAgentHost> host =
        new ServiceWorkerDevToolsAgentHost(id, service_worker_id,
                                           is_installed_version);
    workers_[id] = host.get();
    for (auto& observer : observer_list_)
      observer.WorkerCreated(host.get());
    if (debug_service_worker_on_start_)
        host->PauseForDebugOnStart();
    return host->IsPausedForDebugOnStart();
  }

  // Worker was restarted.
  ServiceWorkerDevToolsAgentHost* agent_host = it->second;
  agent_host->WorkerRestarted(id);
  workers_.erase(it);
  workers_[id] = agent_host;
  return agent_host->IsAttached();
}

void ServiceWorkerDevToolsManager::WorkerReadyForInspection(
    int worker_process_id,
    int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = workers_.find(id);
  if (it == workers_.end())
    return;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host = it->second;
  host->WorkerReadyForInspection();
  for (auto& observer : observer_list_)
    observer.WorkerReadyForInspection(host.get());

  // Then bring up UI for the ones not picked by other clients.
  if (host->IsPausedForDebugOnStart() && !host->IsAttached())
    static_cast<DevToolsAgentHostImpl*>(host.get())->Inspect();
}

void ServiceWorkerDevToolsManager::WorkerVersionInstalled(int worker_process_id,
                                                          int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = workers_.find(id);
  if (it == workers_.end())
    return;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host = it->second;
  host->WorkerVersionInstalled();
  for (auto& observer : observer_list_)
    observer.WorkerVersionInstalled(host.get());
}

void ServiceWorkerDevToolsManager::WorkerVersionDoomed(int worker_process_id,
                                                       int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = workers_.find(id);
  if (it == workers_.end())
    return;
  scoped_refptr<ServiceWorkerDevToolsAgentHost> host = it->second;
  host->WorkerVersionDoomed();
  for (auto& observer : observer_list_)
    observer.WorkerVersionDoomed(host.get());
}

void ServiceWorkerDevToolsManager::WorkerDestroyed(int worker_process_id,
                                                   int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = workers_.find(id);
  if (it == workers_.end())
    return;
  scoped_refptr<WorkerDevToolsAgentHost> agent_host(it->second);
  agent_host->WorkerDestroyed();
  for (auto& observer : observer_list_)
    observer.WorkerDestroyed(it->second);
}

void ServiceWorkerDevToolsManager::RemoveInspectedWorkerData(WorkerId id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  workers_.erase(id);
}

void ServiceWorkerDevToolsManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ServiceWorkerDevToolsManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ServiceWorkerDevToolsManager::set_debug_service_worker_on_start(
    bool debug_on_start) {
  debug_service_worker_on_start_ = debug_on_start;
}

ServiceWorkerDevToolsManager::ServiceWorkerDevToolsManager()
    : debug_service_worker_on_start_(false) {
}

ServiceWorkerDevToolsManager::~ServiceWorkerDevToolsManager() {
}

ServiceWorkerDevToolsManager::AgentHostMap::iterator
ServiceWorkerDevToolsManager::FindExistingWorkerAgentHost(
    const ServiceWorkerIdentifier& service_worker_id) {
  AgentHostMap::iterator it = workers_.begin();
  for (; it != workers_.end(); ++it) {
    if (it->second->Matches(service_worker_id))
      break;
  }
  return it;
}

void ServiceWorkerDevToolsManager::ResetForTesting() {
  workers_.clear();
}

}  // namespace content
