// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_TRACKING_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_TRACKING_H_

#if defined(ENABLE_GPU)

#include "base/basictypes.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"

namespace content {

// All decoders in a context group point to a single GpuMemoryTrackingGroup,
// which tracks GPU resource consumption for the entire context group.
class GpuMemoryTrackingGroup {
 public:
  GpuMemoryTrackingGroup(base::ProcessId pid,
                         gpu::gles2::MemoryTracker* memory_tracker,
                         GpuMemoryManager* memory_manager)
      : pid_(pid),
        size_(0),
        memory_tracker_(memory_tracker),
        memory_manager_(memory_manager) {
    memory_manager_->AddTrackingGroup(this);
  }
  ~GpuMemoryTrackingGroup() {
    memory_manager_->RemoveTrackingGroup(this);
  }
  void TrackMemoryAllocatedChange(
      size_t old_size,
      size_t new_size,
      gpu::gles2::MemoryTracker::Pool tracking_pool) {
    if (old_size < new_size) {
      size_t delta = new_size - old_size;
      size_ += delta;
    }
    if (new_size < old_size) {
      size_t delta = old_size - new_size;
      DCHECK(size_ >= delta);
      size_ -= delta;
    }
    memory_manager_->TrackMemoryAllocatedChange(
        old_size, new_size, tracking_pool);
  }
  base::ProcessId GetPid() const {
    return pid_;
  }
  size_t GetSize() const {
    return size_;
  }
  gpu::gles2::MemoryTracker* GetMemoryTracker() const {
    return memory_tracker_;
  }

 private:
  base::ProcessId pid_;
  size_t size_;
  gpu::gles2::MemoryTracker* memory_tracker_;
  GpuMemoryManager* memory_manager_;
};

}  // namespace content

#endif

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_TRACKING_H_
