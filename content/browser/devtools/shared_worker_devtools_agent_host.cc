// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_agent_host.h"

#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

SharedWorkerDevToolsAgentHost::SharedWorkerDevToolsAgentHost(
    WorkerId worker_id,
    const SharedWorkerInstance& shared_worker)
    : WorkerDevToolsAgentHost(shared_worker.devtools_worker_token(), worker_id),
      shared_worker_(new SharedWorkerInstance(shared_worker)) {
  NotifyCreated();
}

std::string SharedWorkerDevToolsAgentHost::GetType() {
  return kTypeSharedWorker;
}

std::string SharedWorkerDevToolsAgentHost::GetTitle() {
  return shared_worker_->name();
}

GURL SharedWorkerDevToolsAgentHost::GetURL() {
  return shared_worker_->url();
}

bool SharedWorkerDevToolsAgentHost::Activate() {
  return false;
}

void SharedWorkerDevToolsAgentHost::Reload() {
}

bool SharedWorkerDevToolsAgentHost::Close() {
  static_cast<SharedWorkerServiceImpl*>(SharedWorkerService::GetInstance())
      ->TerminateWorkerById(worker_id().first, worker_id().second);
  return true;
}

bool SharedWorkerDevToolsAgentHost::Matches(
    const SharedWorkerInstance& other) {
  return shared_worker_->Matches(other);
}

SharedWorkerDevToolsAgentHost::~SharedWorkerDevToolsAgentHost() {
  SharedWorkerDevToolsManager::GetInstance()->RemoveInspectedWorkerData(
      worker_id());
}

}  // namespace content
