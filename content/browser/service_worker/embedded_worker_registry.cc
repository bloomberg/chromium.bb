// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_registry.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
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

ServiceWorkerStatusCode EmbeddedWorkerRegistry::StartWorker(
    int process_id,
    int embedded_worker_id,
    int64 service_worker_version_id,
    const GURL& scope,
    const GURL& script_url) {
  return Send(
      process_id,
      new EmbeddedWorkerMsg_StartWorker(
          embedded_worker_id, service_worker_version_id, scope, script_url));
}

ServiceWorkerStatusCode EmbeddedWorkerRegistry::StopWorker(
    int process_id, int embedded_worker_id) {
  return Send(process_id,
              new EmbeddedWorkerMsg_StopWorker(embedded_worker_id));
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

void EmbeddedWorkerRegistry::OnSendMessageToBrowser(
    int embedded_worker_id, int request_id, const IPC::Message& message) {
  WorkerInstanceMap::iterator found = worker_map_.find(embedded_worker_id);
  if (found == worker_map_.end()) {
    LOG(ERROR) << "Worker " << embedded_worker_id << " not registered";
    return;
  }
  // Perform security check to filter out any unexpected (and non-test)
  // messages. This must list up all message types that can go through here.
  if (message.type() == ServiceWorkerHostMsg_ActivateEventFinished::ID ||
      message.type() == ServiceWorkerHostMsg_InstallEventFinished::ID ||
      message.type() == ServiceWorkerHostMsg_FetchEventFinished::ID ||
      message.type() == ServiceWorkerHostMsg_SyncEventFinished::ID ||
      IPC_MESSAGE_CLASS(message) == TestMsgStart) {
    found->second->OnMessageReceived(request_id, message);
    return;
  }
  NOTREACHED() << "Got unexpected message: " << message.type();
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

EmbeddedWorkerRegistry::~EmbeddedWorkerRegistry() {}

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
