// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/common/shared_worker/shared_worker_client.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bind_interface_helpers.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"

namespace content {
namespace {

bool IsShuttingDown(RenderProcessHost* host) {
  return !host || host->FastShutdownStarted() ||
         host->IsKeepAliveRefCountDisabled();
}

}  // namespace

SharedWorkerServiceImpl* SharedWorkerServiceImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // OK to just leak the instance.
  // TODO(darin): Consider hanging instances off of StoragePartitionImpl.
  static SharedWorkerServiceImpl* instance = nullptr;
  if (!instance)
    instance = new SharedWorkerServiceImpl();
  return instance;
}

SharedWorkerServiceImpl::SharedWorkerServiceImpl() {}

SharedWorkerServiceImpl::~SharedWorkerServiceImpl() {}

void SharedWorkerServiceImpl::ResetForTesting() {
  worker_hosts_.clear();
}

bool SharedWorkerServiceImpl::TerminateWorker(int process_id, int route_id) {
  SharedWorkerHost* host = FindSharedWorkerHost(process_id, route_id);
  if (!host || !host->instance())
    return false;
  host->TerminateWorker();
  return true;
}

void SharedWorkerServiceImpl::TerminateAllWorkersForTesting(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!terminate_all_workers_callback_);
  if (worker_hosts_.empty()) {
    // Run callback asynchronously to avoid re-entering the caller.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  } else {
    terminate_all_workers_callback_ = std::move(callback);
    for (auto& iter : worker_hosts_)
      iter.second->TerminateWorker();
    // Monitor for actual termination in DestroyHost.
  }
}

void SharedWorkerServiceImpl::ConnectToWorker(
    int process_id,
    int frame_id,
    mojom::SharedWorkerInfoPtr info,
    mojom::SharedWorkerClientPtr client,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    const blink::MessagePortChannel& message_port,
    ResourceContext* resource_context,
    const WorkerStoragePartitionId& partition_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto instance = std::make_unique<SharedWorkerInstance>(
      info->url, info->name, info->content_security_policy,
      info->content_security_policy_type, info->creation_address_space,
      resource_context, partition_id, creation_context_type,
      base::UnguessableToken::Create());

  SharedWorkerHost* host = FindAvailableSharedWorkerHost(*instance);
  if (host) {
    // Non-secure contexts cannot connect to secure workers, and secure contexts
    // cannot connect to non-secure workers:
    if (host->instance()->creation_context_type() != creation_context_type) {
      client->OnScriptLoadFailed();
      return;
    }

    // The process may be shutting down, in which case we will try to create a
    // new shared worker instead.
    if (!IsShuttingDown(RenderProcessHost::FromID(host->process_id()))) {
      host->AddClient(std::move(client), process_id, frame_id, message_port);
      return;
    }
    // Cleanup the existing shared worker now, to avoid having two matching
    // instances. This host would likely be observing the destruction of the
    // child process shortly, but we can clean this up now to avoid some
    // complexity.
    DestroyHost(host->process_id(), host->route_id());
  }

  CreateWorker(std::move(instance), std::move(client), process_id, frame_id,
               message_port);
}

void SharedWorkerServiceImpl::DestroyHost(int process_id, int route_id) {
  worker_hosts_.erase(WorkerID(process_id, route_id));

  // Complete the call to TerminateAllWorkersForTesting if no more workers.
  if (worker_hosts_.empty() && terminate_all_workers_callback_)
    std::move(terminate_all_workers_callback_).Run();

  RenderProcessHost* process_host = RenderProcessHost::FromID(process_id);
  if (!IsShuttingDown(process_host))
    process_host->DecrementKeepAliveRefCount();
}

void SharedWorkerServiceImpl::CreateWorker(
    std::unique_ptr<SharedWorkerInstance> instance,
    mojom::SharedWorkerClientPtr client,
    int process_id,
    int frame_id,
    const blink::MessagePortChannel& message_port) {
  // Re-use the process that requested the shared worker.
  int worker_process_id = process_id;

  RenderProcessHost* process_host = RenderProcessHost::FromID(process_id);
  // If the requesting process is shutting down, then just drop this request on
  // the floor. The client is not going to be around much longer anyways.
  if (IsShuttingDown(process_host))
    return;

  // Keep the renderer process alive that will be hosting the shared worker.
  process_host->IncrementKeepAliveRefCount();

  // TODO(darin): Eliminate the need for shared workers to have routing IDs.
  // Dev Tools will need to be modified to use something else as an identifier.
  int worker_route_id = process_host->GetNextRoutingID();

  bool pause_on_start =
      SharedWorkerDevToolsManager::GetInstance()->WorkerCreated(
          worker_process_id, worker_route_id, *instance);

  auto host = std::make_unique<SharedWorkerHost>(
      std::move(instance), worker_process_id, worker_route_id);

  // Get the factory used to instantiate the new shared worker instance in
  // the target process.
  mojom::SharedWorkerFactoryPtr factory;
  BindInterface(process_host, &factory);

  host->Start(std::move(factory), pause_on_start);
  host->AddClient(std::move(client), process_id, frame_id, message_port);

  const GURL url = host->instance()->url();
  const std::string name = host->instance()->name();

  worker_hosts_[WorkerID(worker_process_id, worker_route_id)] = std::move(host);
}

SharedWorkerHost* SharedWorkerServiceImpl::FindSharedWorkerHost(int process_id,
                                                                int route_id) {
  auto iter = worker_hosts_.find(WorkerID(process_id, route_id));
  if (iter == worker_hosts_.end())
    return nullptr;
  return iter->second.get();
}

SharedWorkerHost* SharedWorkerServiceImpl::FindAvailableSharedWorkerHost(
    const SharedWorkerInstance& instance) {
  for (const auto& iter : worker_hosts_) {
    SharedWorkerHost* host = iter.second.get();
    if (host->IsAvailable() && host->instance()->Matches(instance))
      return host;
  }
  return nullptr;
}

}  // namespace content
