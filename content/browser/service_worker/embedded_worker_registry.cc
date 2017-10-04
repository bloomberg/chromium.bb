// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_registry.h"

#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"

namespace content {

// static
scoped_refptr<EmbeddedWorkerRegistry> EmbeddedWorkerRegistry::Create(
    const base::WeakPtr<ServiceWorkerContextCore>& context) {
  return base::WrapRefCounted(new EmbeddedWorkerRegistry(context, 0));
}

// static
scoped_refptr<EmbeddedWorkerRegistry> EmbeddedWorkerRegistry::Create(
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    EmbeddedWorkerRegistry* old_registry) {
  scoped_refptr<EmbeddedWorkerRegistry> registry =
      new EmbeddedWorkerRegistry(
          context,
          old_registry->next_embedded_worker_id_);
  return registry;
}

std::unique_ptr<EmbeddedWorkerInstance> EmbeddedWorkerRegistry::CreateWorker() {
  std::unique_ptr<EmbeddedWorkerInstance> worker(
      new EmbeddedWorkerInstance(context_, next_embedded_worker_id_));
  worker_map_[next_embedded_worker_id_++] = worker.get();
  return worker;
}

bool EmbeddedWorkerRegistry::OnMessageReceived(const IPC::Message& message,
                                               int process_id) {
  // TODO(kinuko): Move all EmbeddedWorker message handling from
  // ServiceWorkerDispatcherHost.

  EmbeddedWorkerInstance* worker =
      GetWorkerForMessage(process_id, message.routing_id());
  if (!worker) {
    // Assume this is from a detached worker, return true to indicate we're
    // purposely handling the message as no-op.
    return true;
  }
  bool handled = worker->OnMessageReceived(message);

  // Assume an unhandled message for a stopping worker is because the message
  // was timed out and its handler removed prior to stopping.
  // We might be more precise and record timed out request ids, but some
  // cumbersome bookkeeping is needed and the IPC messaging will soon migrate
  // to Mojo anyway.
  return handled || worker->status() == EmbeddedWorkerStatus::STOPPING;
}

void EmbeddedWorkerRegistry::Shutdown() {
  for (WorkerInstanceMap::iterator it = worker_map_.begin();
       it != worker_map_.end();
       ++it) {
    it->second->Stop();
  }
}

bool EmbeddedWorkerRegistry::OnWorkerStarted(int process_id,
                                             int embedded_worker_id) {
  if (!base::ContainsKey(worker_process_map_, process_id) ||
      !base::ContainsKey(worker_process_map_[process_id], embedded_worker_id)) {
    return false;
  }

  lifetime_tracker_.StartTiming(embedded_worker_id);
  return true;
}

void EmbeddedWorkerRegistry::OnWorkerStopped(int process_id,
                                             int embedded_worker_id) {
  worker_process_map_[process_id].erase(embedded_worker_id);
  lifetime_tracker_.StopTiming(embedded_worker_id);
}

void EmbeddedWorkerRegistry::OnDevToolsAttached(int embedded_worker_id) {
  lifetime_tracker_.AbortTiming(embedded_worker_id);
}

EmbeddedWorkerInstance* EmbeddedWorkerRegistry::GetWorker(
    int embedded_worker_id) {
  WorkerInstanceMap::iterator found = worker_map_.find(embedded_worker_id);
  if (found == worker_map_.end())
    return nullptr;
  return found->second;
}

bool EmbeddedWorkerRegistry::CanHandle(int embedded_worker_id) const {
  if (embedded_worker_id < initial_embedded_worker_id_ ||
      next_embedded_worker_id_ <= embedded_worker_id) {
    return false;
  }
  return true;
}

EmbeddedWorkerRegistry::EmbeddedWorkerRegistry(
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    int initial_embedded_worker_id)
    : context_(context),
      next_embedded_worker_id_(initial_embedded_worker_id),
      initial_embedded_worker_id_(initial_embedded_worker_id) {
}

EmbeddedWorkerRegistry::~EmbeddedWorkerRegistry() {
  Shutdown();
}

void EmbeddedWorkerRegistry::BindWorkerToProcess(int process_id,
                                                 int embedded_worker_id) {
  DCHECK(GetWorker(embedded_worker_id));
  DCHECK_EQ(GetWorker(embedded_worker_id)->process_id(), process_id);
  DCHECK(
      !base::ContainsKey(worker_process_map_, process_id) ||
      !base::ContainsKey(worker_process_map_[process_id], embedded_worker_id));

  worker_process_map_[process_id].insert(embedded_worker_id);
}

ServiceWorkerStatusCode EmbeddedWorkerRegistry::Send(
    int process_id, IPC::Message* message_ptr) {
  std::unique_ptr<IPC::Message> message(message_ptr);
  if (!context_)
    return SERVICE_WORKER_ERROR_ABORT;
  IPC::Sender* sender = context_->GetDispatcherHost(process_id);
  if (!sender)
    return SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND;
  if (!sender->Send(message.release()))
    return SERVICE_WORKER_ERROR_IPC_FAILED;
  return SERVICE_WORKER_OK;
}

void EmbeddedWorkerRegistry::RemoveWorker(int process_id,
                                          int embedded_worker_id) {
  DCHECK(base::ContainsKey(worker_map_, embedded_worker_id));
  DetachWorker(process_id, embedded_worker_id);
  worker_map_.erase(embedded_worker_id);
}

void EmbeddedWorkerRegistry::DetachWorker(int process_id,
                                          int embedded_worker_id) {
  DCHECK(base::ContainsKey(worker_map_, embedded_worker_id));
  if (!base::ContainsKey(worker_process_map_, process_id))
    return;
  worker_process_map_[process_id].erase(embedded_worker_id);
  if (worker_process_map_[process_id].empty())
    worker_process_map_.erase(process_id);
  lifetime_tracker_.StopTiming(embedded_worker_id);
}

EmbeddedWorkerInstance* EmbeddedWorkerRegistry::GetWorkerForMessage(
    int process_id,
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = GetWorker(embedded_worker_id);
  if (!worker || worker->process_id() != process_id) {
    UMA_HISTOGRAM_BOOLEAN("ServiceWorker.WorkerForMessageFound", false);
    return nullptr;
  }
  UMA_HISTOGRAM_BOOLEAN("ServiceWorker.WorkerForMessageFound", true);
  return worker;
}

}  // namespace content
