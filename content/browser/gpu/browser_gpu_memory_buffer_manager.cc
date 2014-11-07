// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

BrowserGpuMemoryBufferManager* g_gpu_memory_buffer_manager = nullptr;

// Global atomic to generate gpu memory buffer unique IDs.
base::StaticAtomicSequenceNumber g_next_gpu_memory_buffer_id;

}  // namespace

struct BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferRequest {
  AllocateGpuMemoryBufferRequest(const gfx::Size& size,
                                 gfx::GpuMemoryBuffer::Format format,
                                 gfx::GpuMemoryBuffer::Usage usage,
                                 int client_id)
      : event(true, false),
        size(size),
        format(format),
        usage(usage),
        client_id(client_id) {}
  ~AllocateGpuMemoryBufferRequest() {}
  base::WaitableEvent event;
  gfx::Size size;
  gfx::GpuMemoryBuffer::Format format;
  gfx::GpuMemoryBuffer::Usage usage;
  int client_id;
  scoped_ptr<gfx::GpuMemoryBuffer> result;
};

BrowserGpuMemoryBufferManager::BrowserGpuMemoryBufferManager(int gpu_client_id)
    : gpu_client_id_(gpu_client_id) {
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
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  AllocateGpuMemoryBufferRequest request(size, format, usage, gpu_client_id_);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferOnIO,
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  gfx::GpuMemoryBufferId new_id = g_next_gpu_memory_buffer_id.GetNext();

  BufferMap& buffers = clients_[child_client_id];
  DCHECK(buffers.find(new_id) == buffers.end());

  // Note: Handling of cases where the child process is removed before the
  // allocation completes is less subtle if we set the buffer type to
  // EMPTY_BUFFER here and verify that this has not changed when allocation
  // completes.
  buffers[new_id] = gfx::EMPTY_BUFFER;

  GpuMemoryBufferImpl::AllocateForChildProcess(
      new_id,
      size,
      format,
      usage,
      child_process_handle,
      child_client_id,
      base::Bind(&BrowserGpuMemoryBufferManager::
                     GpuMemoryBufferAllocatedForChildProcess,
                 base::Unretained(this),
                 child_process_handle,
                 child_client_id,
                 callback));
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

void BrowserGpuMemoryBufferManager::ChildProcessDeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    base::ProcessHandle child_process_handle,
    int child_client_id,
    uint32 sync_point) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(clients_.find(child_client_id) != clients_.end());

  BufferMap& buffers = clients_[child_client_id];

  BufferMap::iterator buffer_it = buffers.find(id);
  if (buffer_it == buffers.end()) {
    LOG(ERROR) << "Invalid GpuMemoryBuffer ID for child process.";
    return;
  }

  // This can happen if a child process managed to trigger a call to this while
  // a buffer is in the process of being allocated.
  if (buffer_it->second == gfx::EMPTY_BUFFER) {
    LOG(ERROR) << "Invalid GpuMemoryBuffer type.";
    return;
  }

  GpuMemoryBufferImpl::DeletedByChildProcess(
      buffer_it->second, id, child_process_handle, child_client_id, sync_point);

  buffers.erase(buffer_it);
}

void BrowserGpuMemoryBufferManager::ProcessRemoved(
    base::ProcessHandle process_handle,
    int client_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ClientMap::iterator client_it = clients_.find(client_id);
  if (client_it == clients_.end())
    return;

  for (auto &buffer_it : client_it->second) {
    // This might happen if buffer is currenlty in the process of being
    // allocated. The buffer will in that case be cleaned up when allocation
    // completes.
    if (buffer_it.second == gfx::EMPTY_BUFFER)
      continue;

    GpuMemoryBufferImpl::DeletedByChildProcess(buffer_it.second,
                                               buffer_it.first,
                                               process_handle,
                                               client_id,
                                               0);
  }

  clients_.erase(client_it);
}

void BrowserGpuMemoryBufferManager::GpuMemoryBufferAllocatedForChildProcess(
    base::ProcessHandle child_process_handle,
    int child_client_id,
    const AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ClientMap::iterator client_it = clients_.find(child_client_id);

  // This can happen if the child process is removed while the buffer is being
  // allocated.
  if (client_it == clients_.end()) {
    if (!handle.is_null()) {
      GpuMemoryBufferImpl::DeletedByChildProcess(handle.type,
                                                 handle.id,
                                                 child_process_handle,
                                                 child_client_id,
                                                 0);
    }
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  BufferMap& buffers = client_it->second;

  BufferMap::iterator buffer_it = buffers.find(handle.id);
  DCHECK(buffer_it != buffers.end());
  DCHECK_EQ(buffer_it->second, gfx::EMPTY_BUFFER);

  if (handle.is_null()) {
    buffers.erase(buffer_it);
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  // Store the type for this buffer so it can be cleaned up if the child
  // process is removed.
  buffer_it->second = handle.type;

  callback.Run(handle);
}

// static
void BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferOnIO(
    AllocateGpuMemoryBufferRequest* request) {
  GpuMemoryBufferImpl::Create(
      g_next_gpu_memory_buffer_id.GetNext(),
      request->size,
      request->format,
      request->usage,
      request->client_id,
      base::Bind(&BrowserGpuMemoryBufferManager::GpuMemoryBufferCreatedOnIO,
                 base::Unretained(request)));
}

// static
void BrowserGpuMemoryBufferManager::GpuMemoryBufferCreatedOnIO(
    AllocateGpuMemoryBufferRequest* request,
    scoped_ptr<GpuMemoryBufferImpl> buffer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  request->result = buffer.Pass();
  request->event.Signal();
}

}  // namespace content
