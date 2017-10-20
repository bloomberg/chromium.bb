// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_agent_host.h"

#include "base/strings/stringprintf.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

void StatusNoOp(ServiceWorkerStatusCode status) {}

void TerminateServiceWorkerOnIO(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id))
      version->StopWorker(base::BindOnce(&base::DoNothing));
  }
}

void UnregisterServiceWorkerOnIO(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id)) {
      version->StopWorker(base::BindOnce(&base::DoNothing));
      context->UnregisterServiceWorker(version->scope(),
                                       base::Bind(&StatusNoOp));
    }
  }
}

void SetDevToolsAttachedOnIO(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id,
    bool attached) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id))
      version->SetDevToolsAttached(attached);
  }
}

}  // namespace

ServiceWorkerDevToolsAgentHost::ServiceWorkerDevToolsAgentHost(
    WorkerId worker_id,
    const ServiceWorkerIdentifier& service_worker,
    bool is_installed_version)
    : WorkerDevToolsAgentHost(service_worker.devtools_worker_token(),
                              worker_id),
      service_worker_(new ServiceWorkerIdentifier(service_worker)),
      version_installed_time_(is_installed_version ? base::Time::Now()
                                                   : base::Time()) {
  NotifyCreated();
}

std::string ServiceWorkerDevToolsAgentHost::GetType() {
  return kTypeServiceWorker;
}

std::string ServiceWorkerDevToolsAgentHost::GetTitle() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id().first)) {
    return base::StringPrintf("Worker pid:%" CrPRIdPid,
                              base::GetProcId(host->GetHandle()));
  }
  return "";
}

GURL ServiceWorkerDevToolsAgentHost::GetURL() {
  return service_worker_->url();
}

bool ServiceWorkerDevToolsAgentHost::Activate() {
  return false;
}

void ServiceWorkerDevToolsAgentHost::Reload() {
}

bool ServiceWorkerDevToolsAgentHost::Close() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&TerminateServiceWorkerOnIO,
                                         service_worker_->context_weak(),
                                         service_worker_->version_id()));
  return true;
}

void ServiceWorkerDevToolsAgentHost::UnregisterWorker() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&UnregisterServiceWorkerOnIO,
                                         service_worker_->context_weak(),
                                         service_worker_->version_id()));
}

void ServiceWorkerDevToolsAgentHost::OnAttachedStateChanged(bool attached) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SetDevToolsAttachedOnIO, service_worker_->context_weak(),
                     service_worker_->version_id(), attached));
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionInstalled() {
  version_installed_time_ = base::Time::Now();
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionDoomed() {
  version_doomed_time_ = base::Time::Now();
}

void ServiceWorkerDevToolsAgentHost::NavigationPreloadRequestSent(
    const std::string& request_id,
    const ResourceRequest& request) {
  for (auto* network : protocol::NetworkHandler::ForAgentHost(this)) {
    network->NavigationPreloadRequestSent(worker_id().first, request_id,
                                          request);
  }
}

void ServiceWorkerDevToolsAgentHost::NavigationPreloadResponseReceived(
    const std::string& request_id,
    const GURL& url,
    const ResourceResponseHead& head) {
  for (auto* network : protocol::NetworkHandler::ForAgentHost(this)) {
    network->NavigationPreloadResponseReceived(worker_id().first, request_id,
                                               url, head);
  }
}

void ServiceWorkerDevToolsAgentHost::NavigationPreloadCompleted(
    const std::string& request_id,
    const ResourceRequestCompletionStatus& completion_status) {
  for (auto* network : protocol::NetworkHandler::ForAgentHost(this))
    network->NavigationPreloadCompleted(request_id, completion_status);
}

int64_t ServiceWorkerDevToolsAgentHost::service_worker_version_id() const {
  return service_worker_->version_id();
}

GURL ServiceWorkerDevToolsAgentHost::scope() const {
  return service_worker_->scope();
}

bool ServiceWorkerDevToolsAgentHost::Matches(
    const ServiceWorkerIdentifier& other) {
  return service_worker_->Matches(other);
}

ServiceWorkerDevToolsAgentHost::~ServiceWorkerDevToolsAgentHost() {
  ServiceWorkerDevToolsManager::GetInstance()->RemoveInspectedWorkerData(
      worker_id());
}

}  // namespace content
