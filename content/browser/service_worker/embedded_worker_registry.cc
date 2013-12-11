// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_registry.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/service_worker_messages.h"
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

void EmbeddedWorkerRegistry::RemoveWorker(int embedded_worker_id) {
  DCHECK(ContainsKey(worker_map_, embedded_worker_id));
  worker_map_.erase(embedded_worker_id);
}

bool EmbeddedWorkerRegistry::StartWorker(
    int process_id,
    int embedded_worker_id,
    int64 service_worker_version_id,
    const GURL& script_url) {
  return Send(process_id,
              new ServiceWorkerMsg_StartWorker(embedded_worker_id,
                                               service_worker_version_id,
                                               script_url));
}

bool EmbeddedWorkerRegistry::StopWorker(int process_id,
                                        int embedded_worker_id) {
  return Send(process_id,
              new ServiceWorkerMsg_TerminateWorker(embedded_worker_id));
}

void EmbeddedWorkerRegistry::AddChildProcessSender(
    int process_id, IPC::Sender* sender) {
  process_sender_map_[process_id] = sender;
}

void EmbeddedWorkerRegistry::RemoveChildProcessSender(int process_id) {
  process_sender_map_.erase(process_id);
}

EmbeddedWorkerRegistry::~EmbeddedWorkerRegistry() {}

bool EmbeddedWorkerRegistry::Send(int process_id, IPC::Message* message) {
  if (!context_)
    return false;
  ProcessToSenderMap::iterator found = process_sender_map_.find(process_id);
  if (found == process_sender_map_.end())
    return false;
  return found->second->Send(message);
}

}  // namespace content
