// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_gpu_memory_buffer_manager.h"

#include "content/child/child_thread.h"
#include "content/common/child_process_messages.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

namespace content {
namespace {

void DeletedGpuMemoryBuffer(ThreadSafeSender* sender,
                            gfx::GpuMemoryBufferType type,
                            const gfx::GpuMemoryBufferId& id) {
  TRACE_EVENT0("renderer",
               "ChildGpuMemoryBufferManager::DeletedGpuMemoryBuffer");
  sender->Send(new ChildProcessHostMsg_DeletedGpuMemoryBuffer(type, id));
}

}  // namespace

ChildGpuMemoryBufferManager::ChildGpuMemoryBufferManager(
    ThreadSafeSender* sender)
    : sender_(sender) {
}

ChildGpuMemoryBufferManager::~ChildGpuMemoryBufferManager() {
}

scoped_ptr<gfx::GpuMemoryBuffer>
ChildGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage) {
  TRACE_EVENT2("renderer",
               "ChildGpuMemoryBufferManager::AllocateGpuMemoryBuffer",
               "width",
               size.width(),
               "height",
               size.height());

  gfx::GpuMemoryBufferHandle handle;
  IPC::Message* message = new ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer(
      size.width(), size.height(), format, usage, &handle);
  bool success = sender_->Send(message);
  if (!success)
    return scoped_ptr<gfx::GpuMemoryBuffer>();

  scoped_ptr<GpuMemoryBufferImpl> buffer(GpuMemoryBufferImpl::CreateFromHandle(
      handle,
      size,
      format,
      base::Bind(
          &DeletedGpuMemoryBuffer, sender_, handle.type, handle.global_id)));
  if (!buffer) {
    sender_->Send(new ChildProcessHostMsg_DeletedGpuMemoryBuffer(
        handle.type, handle.global_id));
    return scoped_ptr<gfx::GpuMemoryBuffer>();
  }

  return buffer.Pass();
}

gfx::GpuMemoryBuffer*
ChildGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return GpuMemoryBufferImpl::FromClientBuffer(buffer);
}

}  // namespace content
