// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_GLES2_GPU_MEMORY_TRACKER_H_
#define COMPONENTS_VIEW_MANAGER_GLES2_GPU_MEMORY_TRACKER_H_

#include "gpu/command_buffer/service/memory_tracking.h"

namespace gles2 {

// TODO(fsamuel, rjkroege): This is a stub implementation that needs to be
// completed for proper memory tracking.
class GpuMemoryTracker : public gpu::gles2::MemoryTracker {
 public:
  GpuMemoryTracker();

  // gpu::gles2::MemoryTracker implementation:
  void TrackMemoryAllocatedChange(
      size_t old_size,
      size_t new_size,
      gpu::gles2::MemoryTracker::Pool pool) override;
  bool EnsureGPUMemoryAvailable(size_t size_needed) override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;

 private:
  ~GpuMemoryTracker() override;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryTracker);
};

}  // namespace gles2

#endif  // COMPONENTS_VIEW_MANAGER_GLES2_GPU_MEMORY_TRACKER_H_
