// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_manager.h"

#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
SharedWorkerDevToolsManager* SharedWorkerDevToolsManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<SharedWorkerDevToolsManager>::get();
}

bool SharedWorkerDevToolsManager::WorkerCreated(
    int worker_process_id,
    int worker_route_id,
    const SharedWorkerInstance& instance) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = FindExistingWorkerAgentHost(instance);
  if (it == workers().end()) {
    WorkerDevToolsManager::WorkerCreated(
        id,
        new SharedWorkerDevToolsAgentHost(id, instance));
    return false;
  }
  WorkerRestarted(id, it);
  return true;
}

SharedWorkerDevToolsManager::AgentHostMap::iterator
SharedWorkerDevToolsManager::FindExistingWorkerAgentHost(
    const SharedWorkerInstance& instance) {
  AgentHostMap::iterator it = workers().begin();
  for (; it != workers().end(); ++it) {
    if (static_cast<SharedWorkerDevToolsAgentHost*>(
            it->second)->Matches(instance))
      break;
  }
  return it;
}

SharedWorkerDevToolsManager::SharedWorkerDevToolsManager() {
}

SharedWorkerDevToolsManager::~SharedWorkerDevToolsManager() {
}

}  // namespace content
