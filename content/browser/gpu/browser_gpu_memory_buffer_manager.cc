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

void GpuMemoryBufferAllocatedForChildProcess(
    const BrowserGpuMemoryBufferManager::AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  callback.Run(handle);
}

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

  GpuMemoryBufferImpl::AllocateForChildProcess(
      g_next_gpu_memory_buffer_id.GetNext(),
      size,
      format,
      usage,
      child_process_handle,
      child_client_id,
      base::Bind(&GpuMemoryBufferAllocatedForChildProcess, callback));
}

gfx::GpuMemoryBuffer*
BrowserGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return GpuMemoryBufferImpl::FromClientBuffer(buffer);
}

void BrowserGpuMemoryBufferManager::ChildProcessDeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferType type,
    gfx::GpuMemoryBufferId id,
    base::ProcessHandle child_process_handle,
    int child_client_id,
    uint32 sync_point) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  GpuMemoryBufferImpl::DeletedByChildProcess(
      type, id, child_process_handle, child_client_id, sync_point);
}

void BrowserGpuMemoryBufferManager::ProcessRemoved(
    base::ProcessHandle process_handle) {
  // TODO(reveman): Handle child process removal correctly.
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

void BrowserGpuMemoryBufferManager::SetDestructionSyncPoint(
    gfx::GpuMemoryBuffer* buffer,
    uint32 sync_point) {
  static_cast<GpuMemoryBufferImpl*>(buffer)
      ->set_destruction_sync_point(sync_point);
}

}  // namespace content
