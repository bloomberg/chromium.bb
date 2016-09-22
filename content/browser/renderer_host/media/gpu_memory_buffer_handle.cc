// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/gpu_memory_buffer_handle.h"

#include "content/browser/renderer_host/media/gpu_memory_buffer_tracker.h"

namespace content {

GpuMemoryBufferBufferHandle::GpuMemoryBufferBufferHandle(
    GpuMemoryBufferTracker* tracker)
    : tracker_(tracker) {}
GpuMemoryBufferBufferHandle::~GpuMemoryBufferBufferHandle() = default;

gfx::Size GpuMemoryBufferBufferHandle::dimensions() const {
  return tracker_->dimensions();
}

size_t GpuMemoryBufferBufferHandle::mapped_size() const {
  return tracker_->dimensions().GetArea();
}

void* GpuMemoryBufferBufferHandle::data(int plane) {
  DCHECK_GE(plane, 0);
  DCHECK_LT(plane, static_cast<int>(tracker_->gpu_memory_buffers_.size()));
  DCHECK(tracker_->gpu_memory_buffers_[plane]);
  return tracker_->gpu_memory_buffers_[plane]->memory(0);
}

ClientBuffer GpuMemoryBufferBufferHandle::AsClientBuffer(int plane) {
  DCHECK_GE(plane, 0);
  DCHECK_LT(plane, static_cast<int>(tracker_->gpu_memory_buffers_.size()));
  return tracker_->gpu_memory_buffers_[plane]->AsClientBuffer();
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
base::FileDescriptor GpuMemoryBufferBufferHandle::AsPlatformFile() {
  NOTREACHED();
  return base::FileDescriptor();
}
#endif

}  // namespace content
