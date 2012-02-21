// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_
#pragma once

#include "base/basictypes.h"

// These are memory allocation limits assigned to a context.
// They will change over time, given memory availability, and browser state
// Exceeding these limits for an unreasonable amount of time will cause context
// to be lost.
class GpuMemoryAllocation {
 public:
  enum {
    INVALID_RESOURCE_SIZE = -1
  };

  size_t gpu_resource_size_in_bytes;
  bool has_frontbuffer;
  bool has_backbuffer;

  GpuMemoryAllocation()
      : gpu_resource_size_in_bytes(0),
        has_frontbuffer(false),
        has_backbuffer(false) {
  }

  GpuMemoryAllocation(size_t gpu_resource_size_in_bytes,
                      bool has_frontbuffer,
                      bool has_backbuffer)
      : gpu_resource_size_in_bytes(gpu_resource_size_in_bytes),
        has_frontbuffer(has_frontbuffer),
        has_backbuffer(has_backbuffer) {
  }
};

inline bool operator==(const GpuMemoryAllocation& lhs,
                       const GpuMemoryAllocation& rhs) {
  return lhs.gpu_resource_size_in_bytes == rhs.gpu_resource_size_in_bytes &&
      lhs.has_frontbuffer == rhs.has_frontbuffer &&
      lhs.has_backbuffer == rhs.has_backbuffer;
}

inline bool operator!=(const GpuMemoryAllocation& lhs,
                       const GpuMemoryAllocation& rhs) {
  return !(lhs == rhs);
}

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_
