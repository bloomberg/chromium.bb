// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_memory_buffer_factory_host_impl.h"

#include "base/bind.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace content {

GpuMemoryBufferFactoryHostImpl::GpuMemoryBufferFactoryHostImpl()
    : gpu_host_id_(0), next_create_gpu_memory_buffer_request_id_(0) {
}

GpuMemoryBufferFactoryHostImpl::~GpuMemoryBufferFactoryHostImpl() {
}

void GpuMemoryBufferFactoryHostImpl::CreateGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    unsigned internalformat,
    unsigned usage,
    const CreateGpuMemoryBufferCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  uint32 request_id = next_create_gpu_memory_buffer_request_id_++;
  create_gpu_memory_buffer_requests_[request_id] = callback;

  host->CreateGpuMemoryBuffer(
      handle,
      size,
      internalformat,
      usage,
      base::Bind(&GpuMemoryBufferFactoryHostImpl::OnGpuMemoryBufferCreated,
                 base::Unretained(this),
                 request_id));
}

void GpuMemoryBufferFactoryHostImpl::DestroyGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    int32 sync_point) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host)
    return;

  host->DestroyGpuMemoryBuffer(handle, sync_point);
}

void GpuMemoryBufferFactoryHostImpl::OnGpuMemoryBufferCreated(
    uint32 request_id,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CreateGpuMemoryBufferCallbackMap::iterator iter =
      create_gpu_memory_buffer_requests_.find(request_id);
  DCHECK(iter != create_gpu_memory_buffer_requests_.end());
  iter->second.Run(handle);
  create_gpu_memory_buffer_requests_.erase(iter);
}

}  // namespace content
