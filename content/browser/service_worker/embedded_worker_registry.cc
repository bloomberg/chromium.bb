// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_registry.h"

#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"

namespace content {

EmbeddedWorkerRegistry::EmbeddedWorkerRegistry(
    base::WeakPtr<ServiceWorkerContextCore> context)
    : context_(context),
      next_embedded_worker_id_(0) {}

scoped_ptr<EmbeddedWorkerInstance> EmbeddedWorkerRegistry::CreateWorker() {
  scoped_ptr<EmbeddedWorkerInstance> worker(
      new EmbeddedWorkerInstance(this, next_embedded_worker_id_));
  worker_map_[next_embedded_worker_id_++] = worker.get();
  return worker.Pass();
}

void EmbeddedWorkerRegistry::StartWorker(const std::vector<int>& process_ids,
                                         int embedded_worker_id,
                                         int64 service_worker_version_id,
                                         const GURL& scope,
                                         const GURL& script_url,
                                         const StatusCallback& callback) {
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  context_->process_manager()->AllocateWorkerProcess(
      process_ids,
      script_url,
      base::Bind(&EmbeddedWorkerRegistry::StartWorkerWithProcessId,
                 this,
                 embedded_worker_id,
                 base::Passed(make_scoped_ptr(new EmbeddedWorkerMsg_StartWorker(
                     embedded_worker_id,
                     service_worker_version_id,
                     scope,
                     script_url))),
                 callback));
}

ServiceWorkerStatusCode EmbeddedWorkerRegistry::StopWorker(
    int process_id, int embedded_worker_id) {
  if (context_)
    context_->process_manager()->ReleaseWorkerProcess(process_id);
  return Send(process_id,
              new EmbeddedWorkerMsg_StopWorker(embedded_worker_id));
}

bool EmbeddedWorkerRegistry::OnMessageReceived(const IPC::Message& message) {
  // TODO(kinuko): Move all EmbeddedWorker message handling from
  // ServiceWorkerDispatcherHost.

  WorkerInstanceMap::iterator found = worker_map_.find(message.routing_id());
  if (found == worker_map_.end()) {
    LOG(ERROR) << "Worker " << message.routing_id() << " not registered";
    return false;
  }
  return found->second->OnMessageReceived(message);
}

void EmbeddedWorkerRegistry::Shutdown() {
  for (WorkerInstanceMap::iterator it = worker_map_.begin();
       it != worker_map_.end();
       ++it) {
    it->second->Stop();
  }
}

void EmbeddedWorkerRegistry::OnWorkerStarted(
    int process_id, int thread_id, int embedded_worker_id) {
  DCHECK(!ContainsKey(worker_process_map_, process_id) ||
         worker_process_map_[process_id].count(embedded_worker_id) == 0);
  WorkerInstanceMap::iterator found = worker_map_.find(embedded_worker_id);
  if (found == worker_map_.end()) {
    LOG(ERROR) << "Worker " << embedded_worker_id << " not registered";
    return;
  }
  worker_process_map_[process_id].insert(embedded_worker_id);
  DCHECK_EQ(found->second->process_id(), process_id);
  found->second->OnStarted(thread_id);
}

void EmbeddedWorkerRegistry::OnWorkerStopped(
    int process_id, int embedded_worker_id) {
  WorkerInstanceMap::iterator found = worker_map_.find(embedded_worker_id);
  if (found == worker_map_.end()) {
    LOG(ERROR) << "Worker " << embedded_worker_id << " not registered";
    return;
  }
  DCHECK_EQ(found->second->process_id(), process_id);
  worker_process_map_[process_id].erase(embedded_worker_id);
  found->second->OnStopped();
}

void EmbeddedWorkerRegistry::OnReportException(
    int embedded_worker_id,
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  WorkerInstanceMap::iterator found = worker_map_.find(embedded_worker_id);
  if (found == worker_map_.end()) {
    LOG(ERROR) << "Worker " << embedded_worker_id << " not registered";
    return;
  }
  found->second->OnReportException(
      error_message, line_number, column_number, source_url);
}

void EmbeddedWorkerRegistry::OnReportConsoleMessage(
    int embedded_worker_id,
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  WorkerInstanceMap::iterator found = worker_map_.find(embedded_worker_id);
  if (found == worker_map_.end()) {
    LOG(ERROR) << "Worker " << embedded_worker_id << " not registered";
    return;
  }
  found->second->OnReportConsoleMessage(
      source_identifier, message_level, message, line_number, source_url);
}

void EmbeddedWorkerRegistry::AddChildProcessSender(
    int process_id, IPC::Sender* sender) {
  process_sender_map_[process_id] = sender;
  DCHECK(!ContainsKey(worker_process_map_, process_id));
}

void EmbeddedWorkerRegistry::RemoveChildProcessSender(int process_id) {
  process_sender_map_.erase(process_id);
  std::map<int, std::set<int> >::iterator found =
      worker_process_map_.find(process_id);
  if (found != worker_process_map_.end()) {
    const std::set<int>& worker_set = worker_process_map_[process_id];
    for (std::set<int>::const_iterator it = worker_set.begin();
         it != worker_set.end();
         ++it) {
      int embedded_worker_id = *it;
      DCHECK(ContainsKey(worker_map_, embedded_worker_id));
      worker_map_[embedded_worker_id]->OnStopped();
    }
    worker_process_map_.erase(found);
  }
}

EmbeddedWorkerInstance* EmbeddedWorkerRegistry::GetWorker(
    int embedded_worker_id) {
  WorkerInstanceMap::iterator found = worker_map_.find(embedded_worker_id);
  if (found == worker_map_.end())
    return NULL;
  return found->second;
}

EmbeddedWorkerRegistry::~EmbeddedWorkerRegistry() {
  Shutdown();
}

void EmbeddedWorkerRegistry::StartWorkerWithProcessId(
    int embedded_worker_id,
    scoped_ptr<EmbeddedWorkerMsg_StartWorker> message,
    const StatusCallback& callback,
    ServiceWorkerStatusCode status,
    int process_id) {
  WorkerInstanceMap::const_iterator worker =
      worker_map_.find(embedded_worker_id);
  if (worker == worker_map_.end()) {
    // The Instance was destroyed before it could finish starting.  Undo what
    // we've done so far.
    if (context_)
      context_->process_manager()->ReleaseWorkerProcess(process_id);
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  worker->second->RecordProcessId(process_id, status);

  if (status != SERVICE_WORKER_OK) {
    callback.Run(status);
    return;
  }
  // The ServiceWorkerDispatcherHost is supposed to be created when the process
  // is created, and keep an entry in process_sender_map_ for its whole
  // lifetime.
  DCHECK(ContainsKey(process_sender_map_, process_id));
  callback.Run(Send(process_id, message.release()));
}

ServiceWorkerStatusCode EmbeddedWorkerRegistry::Send(
    int process_id, IPC::Message* message) {
  if (!context_)
    return SERVICE_WORKER_ERROR_ABORT;
  ProcessToSenderMap::iterator found = process_sender_map_.find(process_id);
  if (found == process_sender_map_.end())
    return SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND;
  if (!found->second->Send(message))
    return SERVICE_WORKER_ERROR_IPC_FAILED;
  return SERVICE_WORKER_OK;
}

void EmbeddedWorkerRegistry::RemoveWorker(int process_id,
                                          int embedded_worker_id) {
  DCHECK(ContainsKey(worker_map_, embedded_worker_id));
  worker_map_.erase(embedded_worker_id);
  worker_process_map_.erase(process_id);
}

}  // namespace content
