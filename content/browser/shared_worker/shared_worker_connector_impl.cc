// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_connector_impl.h"

#include "base/memory/ptr_util.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"

namespace content {

// static
void SharedWorkerConnectorImpl::Create(
    int process_id,
    int frame_id,
    mojom::SharedWorkerConnectorRequest request) {
  RenderProcessHost* host = RenderProcessHost::FromID(process_id);
  ResourceContext* resource_context =
      host->GetBrowserContext()->GetResourceContext();
  StoragePartitionImpl* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(host->GetStoragePartition());

  // TODO(darin): Surely there can be a better way to extract a comparable
  // identifier from a StoragePartition instance.
  WorkerStoragePartition worker_storage_partition(
      storage_partition_impl->GetURLRequestContext(),
      storage_partition_impl->GetMediaURLRequestContext(),
      storage_partition_impl->GetAppCacheService(),
      storage_partition_impl->GetQuotaManager(),
      storage_partition_impl->GetFileSystemContext(),
      storage_partition_impl->GetDatabaseTracker(),
      storage_partition_impl->GetIndexedDBContext(),
      storage_partition_impl->GetServiceWorkerContext());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SharedWorkerConnectorImpl::CreateOnIOThread, process_id,
                     frame_id, resource_context, worker_storage_partition,
                     std::move(request)));
}

// static
void SharedWorkerConnectorImpl::CreateOnIOThread(
    int process_id,
    int frame_id,
    ResourceContext* resource_context,
    const WorkerStoragePartition& worker_storage_partition,
    mojom::SharedWorkerConnectorRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeStrongBinding(
      base::WrapUnique(new SharedWorkerConnectorImpl(
          process_id, frame_id, resource_context, worker_storage_partition)),
      std::move(request));
}

SharedWorkerConnectorImpl::SharedWorkerConnectorImpl(
    int process_id,
    int frame_id,
    ResourceContext* resource_context,
    const WorkerStoragePartition& worker_storage_partition)
    : process_id_(process_id),
      frame_id_(frame_id),
      resource_context_(resource_context),
      worker_storage_partition_(worker_storage_partition) {}

void SharedWorkerConnectorImpl::Connect(
    mojom::SharedWorkerInfoPtr info,
    mojom::SharedWorkerClientPtr client,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    mojo::ScopedMessagePipeHandle message_port) {
  SharedWorkerServiceImpl::GetInstance()->CreateWorker(
      process_id_, frame_id_, std::move(info), std::move(client),
      creation_context_type, blink::MessagePortChannel(std::move(message_port)),
      resource_context_, WorkerStoragePartitionId(worker_storage_partition_));
}

}  // namespace content
