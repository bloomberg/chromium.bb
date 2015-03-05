// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_manager.h"

#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/worker_service.h"
#include "ipc/ipc_listener.h"

namespace content {

ServiceWorkerDevToolsManager::ServiceWorkerIdentifier::ServiceWorkerIdentifier(
    const ServiceWorkerContextCore* context,
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64 version_id,
    const GURL& url)
    : context_(context),
      context_weak_(context_weak),
      version_id_(version_id),
      url_(url) {
}

ServiceWorkerDevToolsManager::ServiceWorkerIdentifier::ServiceWorkerIdentifier(
    const ServiceWorkerIdentifier& other)
    : context_(other.context_),
      context_weak_(other.context_weak_),
      version_id_(other.version_id_),
      url_(other.url_) {
}

ServiceWorkerDevToolsManager::
ServiceWorkerIdentifier::~ServiceWorkerIdentifier() {
}

bool ServiceWorkerDevToolsManager::ServiceWorkerIdentifier::Matches(
    const ServiceWorkerIdentifier& other) const {
  return context_ == other.context_ && version_id_ == other.version_id_;
}

// static
ServiceWorkerDevToolsManager* ServiceWorkerDevToolsManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<ServiceWorkerDevToolsManager>::get();
}

bool ServiceWorkerDevToolsManager::WorkerCreated(
    int worker_process_id,
    int worker_route_id,
    const ServiceWorkerIdentifier& service_worker_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = FindExistingWorkerAgentHost(service_worker_id);
  if (it == workers().end()) {
    WorkerDevToolsManager::WorkerCreated(id,
        new ServiceWorkerDevToolsAgentHost(id, service_worker_id,
                                           debug_service_worker_on_start_));
    return debug_service_worker_on_start_;
  }
  WorkerRestarted(id, it);
  return it->second->IsAttached();
}

void ServiceWorkerDevToolsManager::WorkerStopIgnored(int worker_process_id,
                                                      int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(pfeldman): Show a console message to tell the user that UA didn't
  // terminate the worker because devtools is attached.
}

ServiceWorkerDevToolsManager::ServiceWorkerDevToolsManager()
    : debug_service_worker_on_start_(false) {
}

ServiceWorkerDevToolsManager::~ServiceWorkerDevToolsManager() {
}

ServiceWorkerDevToolsManager::AgentHostMap::iterator
ServiceWorkerDevToolsManager::FindExistingWorkerAgentHost(
    const ServiceWorkerIdentifier& service_worker_id) {
  AgentHostMap::iterator it = workers().begin();
  for (; it != workers().end(); ++it) {
    if (static_cast<ServiceWorkerDevToolsAgentHost*>(
            it->second)->Matches(service_worker_id))
      break;
  }
  return it;
}

}  // namespace content
