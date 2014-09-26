// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/embedded_worker_devtools_manager.h"

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
    const ServiceWorkerContextCore* context,
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64 version_id,
    const GURL& url)
    : context_(context),
      context_weak_(context_weak),
      version_id_(version_id),
      url_(url) {
}

EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::ServiceWorkerIdentifier(
    const ServiceWorkerIdentifier& other)
    : context_(other.context_),
      context_weak_(other.context_weak_),
      version_id_(other.version_id_),
      url_(other.url_) {
}

EmbeddedWorkerDevToolsManager::
ServiceWorkerIdentifier::~ServiceWorkerIdentifier() {
}

bool EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::Matches(
    const ServiceWorkerIdentifier& other) const {
  return context_ == other.context_ && version_id_ == other.version_id_;
}

const ServiceWorkerContextCore*
EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::context() const {
  return context_;
}

base::WeakPtr<ServiceWorkerContextCore>
EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::context_weak() const {
  return context_weak_;
}

int64
EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::version_id() const {
  return version_id_;
}

GURL EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::url() const {
  return url_;
}

// static
EmbeddedWorkerDevToolsManager* EmbeddedWorkerDevToolsManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<EmbeddedWorkerDevToolsManager>::get();
}

DevToolsAgentHostImpl*
EmbeddedWorkerDevToolsManager::GetDevToolsAgentHostForWorker(
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

void EmbeddedWorkerDevToolsManager::WorkerReadyForInspection(
    int worker_process_id,
    int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  AgentHostMap::iterator it = workers_.find(id);
  DCHECK(it != workers_.end());
  it->second->WorkerReadyForInspection();
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

DevToolsAgentHost::List
EmbeddedWorkerDevToolsManager::GetOrCreateAllAgentHosts() {
  DevToolsAgentHost::List result;
  EmbeddedWorkerDevToolsManager* instance = GetInstance();
  for (AgentHostMap::iterator it = instance->workers_.begin();
      it != instance->workers_.end(); ++it) {
    if (!it->second->IsTerminated())
      result.push_back(it->second);
  }
  return result;
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
