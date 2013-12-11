// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include "content/browser/service_worker/embedded_worker_registry.h"
#include "url/gurl.h"

namespace content {

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  registry_->RemoveWorker(embedded_worker_id_);
}

bool EmbeddedWorkerInstance::Start(
    int64 service_worker_version_id,
    const GURL& script_url) {
  DCHECK(status_ == STOPPED);
  if (!ChooseProcess())
    return false;
  status_ = STARTING;
  bool success = registry_->StartWorker(
      process_id_,
      embedded_worker_id_,
      service_worker_version_id,
      script_url);
  if (!success) {
    status_ = STOPPED;
    process_id_ = -1;
  }
  return success;
}

bool EmbeddedWorkerInstance::Stop() {
  DCHECK(status_ == STARTING || status_ == RUNNING);
  const bool success = registry_->StopWorker(process_id_, embedded_worker_id_);
  if (success)
    status_ = STOPPING;
  return success;
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
  DCHECK(status_ == STARTING);
  status_ = RUNNING;
  thread_id_ = thread_id;
}

void EmbeddedWorkerInstance::OnStopped() {
  status_ = STOPPED;
  process_id_ = -1;
  thread_id_ = -1;
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
