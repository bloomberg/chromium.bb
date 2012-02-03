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
  uint64 gpuResourceSizeInBytes;
  bool hasFrontbuffer;
  bool hasBackbuffer;

  GpuMemoryAllocation()
      : gpuResourceSizeInBytes(0),
        hasFrontbuffer(false),
        hasBackbuffer(false) {
  }

  GpuMemoryAllocation(uint64 gpuResourceSizeInBytes,
                      bool hasFrontbuffer,
                      bool hasBackbuffer)
      : gpuResourceSizeInBytes(gpuResourceSizeInBytes),
        hasFrontbuffer(hasFrontbuffer),
        hasBackbuffer(hasBackbuffer) {
  }
};

inline bool operator==(const GpuMemoryAllocation& lhs,
                       const GpuMemoryAllocation& rhs) {
  return lhs.gpuResourceSizeInBytes == rhs.gpuResourceSizeInBytes &&
      lhs.hasFrontbuffer == rhs.hasFrontbuffer &&
      lhs.hasBackbuffer == rhs.hasBackbuffer;
}

inline bool operator!=(const GpuMemoryAllocation& lhs,
                       const GpuMemoryAllocation& rhs) {
  return !(lhs == rhs);
}

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_
