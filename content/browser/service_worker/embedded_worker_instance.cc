// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

namespace content {

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  registry_->RemoveWorker(process_id_, embedded_worker_id_);
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::Start(
    int64 service_worker_version_id,
    const GURL& scope,
    const GURL& script_url) {
  DCHECK(status_ == STOPPED);
  if (!ChooseProcess())
    return SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND;
  status_ = STARTING;
  ServiceWorkerStatusCode status =
      registry_->StartWorker(process_id_,
                             embedded_worker_id_,
                             service_worker_version_id,
                             scope,
                             script_url);
  if (status != SERVICE_WORKER_OK) {
    status_ = STOPPED;
    process_id_ = -1;
  }
  return status;
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::Stop() {
  DCHECK(status_ == STARTING || status_ == RUNNING);
  ServiceWorkerStatusCode status =
      registry_->StopWorker(process_id_, embedded_worker_id_);
  if (status == SERVICE_WORKER_OK)
    status_ = STOPPING;
  return status;
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::SendMessage(
    int request_id,
    const IPC::Message& message) {
  DCHECK(status_ == RUNNING);
  return registry_->Send(process_id_,
                         new EmbeddedWorkerContextMsg_SendMessageToWorker(
                             thread_id_, embedded_worker_id_,
                             request_id, message));
}

void EmbeddedWorkerInstance::AddProcessReference(int process_id) {
  ProcessRefMap::iterator found = process_refs_.find(process_id);
  if (found == process_refs_.end())
    found = process_refs_.insert(std::make_pair(process_id, 0)).first;
  ++found->second;
}

void EmbeddedWorkerInstance::ReleaseProcessReference(int process_id) {
  ProcessRefMap::iterator found = process_refs_.find(process_id);
  if (found == process_refs_.end()) {
    NOTREACHED() << "Releasing unknown process ref " << process_id;
    return;
  }
  if (--found->second == 0)
    process_refs_.erase(found);
}

EmbeddedWorkerInstance::EmbeddedWorkerInstance(
    EmbeddedWorkerRegistry* registry,
    int embedded_worker_id)
    : registry_(registry),
      embedded_worker_id_(embedded_worker_id),
      status_(STOPPED),
      process_id_(-1),
      thread_id_(-1) {
}

void EmbeddedWorkerInstance::OnStarted(int thread_id) {
  // Stop is requested before OnStarted is sent back from the worker.
  if (status_ == STOPPING)
    return;
  DCHECK(status_ == STARTING);
  status_ = RUNNING;
  thread_id_ = thread_id;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnStarted());
}

void EmbeddedWorkerInstance::OnStopped() {
  status_ = STOPPED;
  process_id_ = -1;
  thread_id_ = -1;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnStopped());
}

void EmbeddedWorkerInstance::OnMessageReceived(int request_id,
                                               const IPC::Message& message) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnMessageReceived(request_id, message));
}

void EmbeddedWorkerInstance::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Observer,
      observer_list_,
      OnReportException(error_message, line_number, column_number, source_url));
}

void EmbeddedWorkerInstance::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EmbeddedWorkerInstance::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool EmbeddedWorkerInstance::ChooseProcess() {
  DCHECK_EQ(-1, process_id_);
  // Naive implementation; chooses a process which has the biggest number of
  // associated providers (so that hopefully likely live longer).
  ProcessRefMap::iterator max_ref_iter = process_refs_.end();
  for (ProcessRefMap::iterator iter = process_refs_.begin();
       iter != process_refs_.end(); ++iter) {
    if (max_ref_iter == process_refs_.end() ||
        max_ref_iter->second < iter->second)
      max_ref_iter = iter;
  }
  if (max_ref_iter == process_refs_.end())
    return false;
  process_id_ = max_ref_iter->first;
  return true;
}

}  // namespace content
