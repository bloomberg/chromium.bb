// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/gles2/gpu_memory_tracker.h"

namespace gles2 {

GpuMemoryTracker::GpuMemoryTracker() {
}

void GpuMemoryTracker::TrackMemoryAllocatedChange(
    size_t old_size,
    size_t new_size,
    gpu::gles2::MemoryTracker::Pool pool) {
}

bool GpuMemoryTracker::EnsureGPUMemoryAvailable(size_t size_needed) {
  return true;
}

uint64_t GpuMemoryTracker::ClientTracingId() const {
  return 0u;
}

int GpuMemoryTracker::ClientId() const {
  return 0;
}

GpuMemoryTracker::~GpuMemoryTracker() {
}

}  // namespace gles2
