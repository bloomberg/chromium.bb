// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worker_devtools_manager.h"

#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/devtools/worker_devtools_agent_host.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// Called on the UI thread.
// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForWorker(
    int worker_process_id,
    int worker_route_id) {
  if (scoped_refptr<DevToolsAgentHost> host =
      SharedWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id)) {
    return host;
  }
  return ServiceWorkerDevToolsManager::GetInstance()
      ->GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id);
}

DevToolsAgentHostImpl*
WorkerDevToolsManager::GetDevToolsAgentHostForWorker(int worker_process_id,
                                                     int worker_route_id) {
  AgentHostMap::iterator it = workers_.find(
      WorkerId(worker_process_id, worker_route_id));
  return it == workers_.end() ? NULL : it->second;
}

void WorkerDevToolsManager::AddAllAgentHosts(DevToolsAgentHost::List* result) {
  for (auto& worker : workers_) {
    if (!worker.second->IsTerminated())
      result->push_back(worker.second);
  }
}

void WorkerDevToolsManager::WorkerDestroyed(int worker_process_id,
                                            int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = workers_.find(id);
  DCHECK(it != workers_.end());
  scoped_refptr<WorkerDevToolsAgentHost> agent_host(it->second);
  FOR_EACH_OBSERVER(Observer, observer_list_, WorkerDestroyed(it->second));
  agent_host->WorkerDestroyed();
  DevToolsManager::GetInstance()->AgentHostChanged(agent_host);
}

void WorkerDevToolsManager::WorkerReadyForInspection(int worker_process_id,
                                                     int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = workers_.find(id);
  DCHECK(it != workers_.end());
  it->second->WorkerReadyForInspection();
}

void WorkerDevToolsManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void WorkerDevToolsManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

WorkerDevToolsManager::WorkerDevToolsManager() {
}

WorkerDevToolsManager::~WorkerDevToolsManager() {
}

void WorkerDevToolsManager::RemoveInspectedWorkerData(WorkerId id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  workers_.erase(id);
}

void WorkerDevToolsManager::WorkerCreated(
    const WorkerId& id,
    WorkerDevToolsAgentHost* host) {
  workers_[id] = host;
  DevToolsManager::GetInstance()->AgentHostChanged(host);
  scoped_refptr<WorkerDevToolsAgentHost> protector(host);
  FOR_EACH_OBSERVER(Observer, observer_list_, WorkerCreated(host));
}

void WorkerDevToolsManager::WorkerRestarted(const WorkerId& id,
                                            const AgentHostMap::iterator& it) {
  WorkerDevToolsAgentHost* agent_host = it->second;
  agent_host->WorkerRestarted(id);
  workers_.erase(it);
  workers_[id] = agent_host;
  DevToolsManager::GetInstance()->AgentHostChanged(agent_host);
}

void WorkerDevToolsManager::ResetForTesting() {
  workers_.clear();
}

}  // namespace content
