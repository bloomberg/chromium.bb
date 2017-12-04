// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_manager.h"

#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
SharedWorkerDevToolsManager* SharedWorkerDevToolsManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<SharedWorkerDevToolsManager>::get();
}

void SharedWorkerDevToolsManager::AddAllAgentHosts(
    SharedWorkerDevToolsAgentHost::List* result) {
  for (auto& it : live_hosts_)
    result->push_back(it.second.get());
}

bool SharedWorkerDevToolsManager::WorkerCreated(SharedWorkerHost* worker_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(live_hosts_.find(worker_host) == live_hosts_.end());

  auto it =
      std::find_if(terminated_hosts_.begin(), terminated_hosts_.end(),
                   [&worker_host](SharedWorkerDevToolsAgentHost* agent_host) {
                     return agent_host->Matches(worker_host);
                   });
  if (it == terminated_hosts_.end()) {
    live_hosts_[worker_host] = new SharedWorkerDevToolsAgentHost(worker_host);
    return false;
  }

  SharedWorkerDevToolsAgentHost* agent_host = *it;
  terminated_hosts_.erase(it);
  live_hosts_[worker_host] = agent_host;
  return agent_host->WorkerRestarted(worker_host);
}

void SharedWorkerDevToolsManager::WorkerReadyForInspection(
    SharedWorkerHost* worker_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  live_hosts_[worker_host]->WorkerReadyForInspection();
}

void SharedWorkerDevToolsManager::WorkerDestroyed(
    SharedWorkerHost* worker_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<SharedWorkerDevToolsAgentHost> agent_host =
      live_hosts_[worker_host];
  live_hosts_.erase(worker_host);
  terminated_hosts_.insert(agent_host.get());
  agent_host->WorkerDestroyed();
}

void SharedWorkerDevToolsManager::AgentHostDestroyed(
    SharedWorkerDevToolsAgentHost* agent_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  terminated_hosts_.erase(agent_host);
}

SharedWorkerDevToolsManager::SharedWorkerDevToolsManager() {
}

SharedWorkerDevToolsManager::~SharedWorkerDevToolsManager() {
}

}  // namespace content
