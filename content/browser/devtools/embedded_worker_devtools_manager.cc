// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/embedded_worker_devtools_manager.h"

#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/embedded_worker_devtools_agent_host.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/worker_service.h"
#include "ipc/ipc_listener.h"

namespace content {

// Called on the UI thread.
// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForWorker(
    int worker_process_id,
    int worker_route_id) {
  return EmbeddedWorkerDevToolsManager::GetInstance()
      ->GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id);
}

EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::ServiceWorkerIdentifier(
    const ServiceWorkerContextCore* const service_worker_context,
    int64 service_worker_version_id)
    : service_worker_context_(service_worker_context),
      service_worker_version_id_(service_worker_version_id) {
}

EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::ServiceWorkerIdentifier(
    const ServiceWorkerIdentifier& other)
    : service_worker_context_(other.service_worker_context_),
      service_worker_version_id_(other.service_worker_version_id_) {
}

bool EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::Matches(
    const ServiceWorkerIdentifier& other) const {
  return service_worker_context_ == other.service_worker_context_ &&
         service_worker_version_id_ == other.service_worker_version_id_;
}

// static
EmbeddedWorkerDevToolsManager* EmbeddedWorkerDevToolsManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<EmbeddedWorkerDevToolsManager>::get();
}

DevToolsAgentHost* EmbeddedWorkerDevToolsManager::GetDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  AgentHostMap::iterator it = workers_.find(
      WorkerId(worker_process_id, worker_route_id));
  return it == workers_.end() ? NULL : it->second;
}

EmbeddedWorkerDevToolsManager::EmbeddedWorkerDevToolsManager()
    : debug_service_worker_on_start_(false) {
}

EmbeddedWorkerDevToolsManager::~EmbeddedWorkerDevToolsManager() {
}

bool EmbeddedWorkerDevToolsManager::SharedWorkerCreated(
    int worker_process_id,
    int worker_route_id,
    const SharedWorkerInstance& instance) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = FindExistingSharedWorkerAgentHost(instance);
  if (it == workers_.end()) {
    workers_[id] = new EmbeddedWorkerDevToolsAgentHost(id, instance);
    return false;
  }
  WorkerRestarted(id, it);
  return true;
}

bool EmbeddedWorkerDevToolsManager::ServiceWorkerCreated(
    int worker_process_id,
    int worker_route_id,
    const ServiceWorkerIdentifier& service_worker_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it =
      FindExistingServiceWorkerAgentHost(service_worker_id);
  if (it == workers_.end()) {
    workers_[id] = new EmbeddedWorkerDevToolsAgentHost(
        id, service_worker_id, debug_service_worker_on_start_);
    return debug_service_worker_on_start_;
  }
  WorkerRestarted(id, it);
  return true;
}

void EmbeddedWorkerDevToolsManager::WorkerDestroyed(int worker_process_id,
                                                    int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = workers_.find(id);
  DCHECK(it != workers_.end());
  it->second->WorkerDestroyed();
}

void EmbeddedWorkerDevToolsManager::WorkerContextStarted(int worker_process_id,
                                                         int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = workers_.find(id);
  DCHECK(it != workers_.end());
  it->second->WorkerContextStarted();
}

void EmbeddedWorkerDevToolsManager::RemoveInspectedWorkerData(WorkerId id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  workers_.erase(id);
}

EmbeddedWorkerDevToolsManager::AgentHostMap::iterator
EmbeddedWorkerDevToolsManager::FindExistingSharedWorkerAgentHost(
    const SharedWorkerInstance& instance) {
  AgentHostMap::iterator it = workers_.begin();
  for (; it != workers_.end(); ++it) {
    if (it->second->Matches(instance))
      break;
  }
  return it;
}

EmbeddedWorkerDevToolsManager::AgentHostMap::iterator
EmbeddedWorkerDevToolsManager::FindExistingServiceWorkerAgentHost(
    const ServiceWorkerIdentifier& service_worker_id) {
  AgentHostMap::iterator it = workers_.begin();
  for (; it != workers_.end(); ++it) {
    if (it->second->Matches(service_worker_id))
      break;
  }
  return it;
}

void EmbeddedWorkerDevToolsManager::WorkerRestarted(
    const WorkerId& id,
    const AgentHostMap::iterator& it) {
  EmbeddedWorkerDevToolsAgentHost* agent_host = it->second;
  agent_host->WorkerRestarted(id);
  workers_.erase(it);
  workers_[id] = agent_host;
}

void EmbeddedWorkerDevToolsManager::ResetForTesting() {
  workers_.clear();
}

}  // namespace content
