// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/client/gpu_memory_buffer_factory_host.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

BrowserGpuMemoryBufferManager* g_gpu_memory_buffer_manager = nullptr;

// Global atomic to generate gpu memory buffer unique IDs.
base::StaticAtomicSequenceNumber g_next_gpu_memory_buffer_id;

const char kMemoryAllocatorName[] = "gpumemorybuffer";

}  // namespace

struct BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferRequest {
  AllocateGpuMemoryBufferRequest(const gfx::Size& size,
                                 gfx::GpuMemoryBuffer::Format format,
                                 gfx::GpuMemoryBuffer::Usage usage,
                                 int client_id,
                                 int surface_id)
      : event(true, false),
        size(size),
        format(format),
        usage(usage),
        client_id(client_id),
        surface_id(surface_id) {}
  ~AllocateGpuMemoryBufferRequest() {}
  base::WaitableEvent event;
  gfx::Size size;
  gfx::GpuMemoryBuffer::Format format;
  gfx::GpuMemoryBuffer::Usage usage;
  int client_id;
  int surface_id;
  scoped_ptr<gfx::GpuMemoryBuffer> result;
};

BrowserGpuMemoryBufferManager::BrowserGpuMemoryBufferManager(
    GpuMemoryBufferFactoryHost* gpu_memory_buffer_factory_host,
    int gpu_client_id)
    : gpu_memory_buffer_factory_host_(gpu_memory_buffer_factory_host),
      gpu_client_id_(gpu_client_id),
      weak_ptr_factory_(this) {
  DCHECK(!g_gpu_memory_buffer_manager);
  g_gpu_memory_buffer_manager = this;
}

BrowserGpuMemoryBufferManager::~BrowserGpuMemoryBufferManager() {
  g_gpu_memory_buffer_manager = nullptr;
}

// static
BrowserGpuMemoryBufferManager* BrowserGpuMemoryBufferManager::current() {
  return g_gpu_memory_buffer_manager;
}

scoped_ptr<gfx::GpuMemoryBuffer>
BrowserGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage) {
  return AllocateGpuMemoryBufferCommon(size, format, usage, 0);
}

scoped_ptr<gfx::GpuMemoryBuffer>
BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForScanout(
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    int32 surface_id) {
  DCHECK_GT(surface_id, 0);
  return AllocateGpuMemoryBufferCommon(
      size, format, gfx::GpuMemoryBuffer::SCANOUT, surface_id);
}

scoped_ptr<gfx::GpuMemoryBuffer>
BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferCommon(
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int32 surface_id) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Fallback to shared memory buffer if |format| and |usage| are not supported
  // by factory.
  if (!gpu_memory_buffer_factory_host_->IsGpuMemoryBufferConfigurationSupported(
          format, usage)) {
    DCHECK(GpuMemoryBufferImplSharedMemory::IsFormatSupported(format))
        << format;
    DCHECK(GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage)) << usage;
    return GpuMemoryBufferImplSharedMemory::Create(
        g_next_gpu_memory_buffer_id.GetNext(), size, format);
  }

  AllocateGpuMemoryBufferRequest request(size, format, usage, gpu_client_id_,
                                         surface_id);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferOnIO,
                 base::Unretained(this),  // Safe as we wait for result below.
                 base::Unretained(&request)));

  // We're blocking the UI thread, which is generally undesirable.
  TRACE_EVENT0("browser",
               "BrowserGpuMemoryBufferManager::AllocateGpuMemoryBuffer");
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  request.event.Wait();
  return request.result.Pass();
}

void BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForChildProcess(
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    base::ProcessHandle child_process_handle,
    int child_client_id,
    const AllocationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  gfx::GpuMemoryBufferId new_id = g_next_gpu_memory_buffer_id.GetNext();

  BufferMap& buffers = clients_[child_client_id];
  DCHECK(buffers.find(new_id) == buffers.end());

  // Fallback to shared memory buffer if |format| and |usage| are not supported
  // by factory.
  if (!gpu_memory_buffer_factory_host_->IsGpuMemoryBufferConfigurationSupported(
          format, usage)) {
    // Early out if we cannot fallback to shared memory buffer.
    if (!GpuMemoryBufferImplSharedMemory::IsFormatSupported(format) ||
        !GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage) ||
        !GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(size, format)) {
      callback.Run(gfx::GpuMemoryBufferHandle());
      return;
    }

    buffers[new_id] = BufferInfo(size, format, gfx::SHARED_MEMORY_BUFFER);
    callback.Run(GpuMemoryBufferImplSharedMemory::AllocateForChildProcess(
        new_id, size, format, child_process_handle));
    return;
  }

  // Note: Handling of cases where the child process is removed before the
  // allocation completes is less subtle if we set the buffer type to
  // EMPTY_BUFFER here and verify that this has not changed when allocation
  // completes.
  buffers[new_id] = BufferInfo(size, format, gfx::EMPTY_BUFFER);

  gpu_memory_buffer_factory_host_->CreateGpuMemoryBuffer(
      new_id, size, format, usage, child_client_id, 0,
      base::Bind(&BrowserGpuMemoryBufferManager::
                     GpuMemoryBufferAllocatedForChildProcess,
                 weak_ptr_factory_.GetWeakPtr(), child_client_id, callback));
}

gfx::GpuMemoryBuffer*
BrowserGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return GpuMemoryBufferImpl::FromClientBuffer(buffer);
}

void BrowserGpuMemoryBufferManager::SetDestructionSyncPoint(
    gfx::GpuMemoryBuffer* buffer,
    uint32 sync_point) {
  static_cast<GpuMemoryBufferImpl*>(buffer)
      ->set_destruction_sync_point(sync_point);
}

bool BrowserGpuMemoryBufferManager::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& client : clients_) {
    for (const auto& buffer : client.second) {
      if (buffer.second.type == gfx::EMPTY_BUFFER)
        continue;

      base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
          base::StringPrintf("%s/%d", kMemoryAllocatorName, buffer.first));
      if (!dump)
        return false;

      size_t buffer_size_in_bytes = 0;
      // Note: BufferSizeInBytes returns an approximated size for the buffer
      // but the factory can be made to return the exact size if this
      // approximation is not good enough.
      bool valid_size = GpuMemoryBufferImpl::BufferSizeInBytes(
          buffer.second.size, buffer.second.format, &buffer_size_in_bytes);
      DCHECK(valid_size);

      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      buffer_size_in_bytes);
    }
  }

  return true;
}

void BrowserGpuMemoryBufferManager::ChildProcessDeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    base::ProcessHandle child_process_handle,
    int child_client_id,
    uint32 sync_point) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(clients_.find(child_client_id) != clients_.end());

  BufferMap& buffers = clients_[child_client_id];

  BufferMap::iterator buffer_it = buffers.find(id);
  if (buffer_it == buffers.end()) {
    LOG(ERROR) << "Invalid GpuMemoryBuffer ID for child process.";
    return;
  }

  // This can happen if a child process managed to trigger a call to this while
  // a buffer is in the process of being allocated.
  if (buffer_it->second.type == gfx::EMPTY_BUFFER) {
    LOG(ERROR) << "Invalid GpuMemoryBuffer type.";
    return;
  }

  // Buffers allocated using the factory need to be destroyed through the
  // factory.
  if (buffer_it->second.type != gfx::SHARED_MEMORY_BUFFER) {
    gpu_memory_buffer_factory_host_->DestroyGpuMemoryBuffer(id,
                                                            child_client_id,
                                                            sync_point);
  }

  buffers.erase(buffer_it);
}

void BrowserGpuMemoryBufferManager::ProcessRemoved(
    base::ProcessHandle process_handle,
    int client_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ClientMap::iterator client_it = clients_.find(client_id);
  if (client_it == clients_.end())
    return;

  for (const auto& buffer : client_it->second) {
    // This might happen if buffer is currenlty in the process of being
    // allocated. The buffer will in that case be cleaned up when allocation
    // completes.
    if (buffer.second.type == gfx::EMPTY_BUFFER)
      continue;

    // Skip shared memory buffers as they were not allocated using the factory.
    if (buffer.second.type == gfx::SHARED_MEMORY_BUFFER)
      continue;

    gpu_memory_buffer_factory_host_->DestroyGpuMemoryBuffer(buffer.first,
                                                            client_id, 0);
  }

  clients_.erase(client_it);
}

void BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferOnIO(
    AllocateGpuMemoryBufferRequest* request) {
  // Note: Unretained is safe as this is only used for synchronous allocation
  // from a non-IO thread.
  gpu_memory_buffer_factory_host_->CreateGpuMemoryBuffer(
      g_next_gpu_memory_buffer_id.GetNext(), request->size, request->format,
      request->usage, request->client_id, request->surface_id,
      base::Bind(&BrowserGpuMemoryBufferManager::GpuMemoryBufferAllocatedOnIO,
                 base::Unretained(this), base::Unretained(request)));
}

void BrowserGpuMemoryBufferManager::GpuMemoryBufferAllocatedOnIO(
    AllocateGpuMemoryBufferRequest* request,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Early out if factory failed to allocate the buffer.
  if (handle.is_null()) {
    request->event.Signal();
    return;
  }

  DCHECK_NE(handle.type, gfx::SHARED_MEMORY_BUFFER);
  request->result = GpuMemoryBufferImpl::CreateFromHandle(
      handle, request->size, request->format, request->usage,
      base::Bind(&BrowserGpuMemoryBufferManager::GpuMemoryBufferDeleted,
                 weak_ptr_factory_.GetWeakPtr(), handle.id,
                 request->client_id));
  request->event.Signal();
}

void BrowserGpuMemoryBufferManager::GpuMemoryBufferDeleted(
    gfx::GpuMemoryBufferId id,
    int client_id,
    uint32 sync_point) {
  gpu_memory_buffer_factory_host_->DestroyGpuMemoryBuffer(id,
                                                          client_id,
                                                          sync_point);
}

void BrowserGpuMemoryBufferManager::GpuMemoryBufferAllocatedForChildProcess(
    int child_client_id,
    const AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ClientMap::iterator client_it = clients_.find(child_client_id);

  // This can happen if the child process is removed while the buffer is being
  // allocated.
  if (client_it == clients_.end()) {
    if (!handle.is_null()) {
      gpu_memory_buffer_factory_host_->DestroyGpuMemoryBuffer(
          handle.id, child_client_id, 0);
    }
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  BufferMap& buffers = client_it->second;

  BufferMap::iterator buffer_it = buffers.find(handle.id);
  DCHECK(buffer_it != buffers.end());
  DCHECK_EQ(buffer_it->second.type, gfx::EMPTY_BUFFER);

  if (handle.is_null()) {
    buffers.erase(buffer_it);
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  // The factory should never return a shared memory backed buffer.
  DCHECK_NE(handle.type, gfx::SHARED_MEMORY_BUFFER);

  // Store the type of this buffer so it can be cleaned up if the child
  // process is removed.
  buffer_it->second.type = handle.type;

  callback.Run(handle);
}

}  // namespace content
