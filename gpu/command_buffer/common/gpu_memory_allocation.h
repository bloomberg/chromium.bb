// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_ALLOCATION_H_
#define GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_ALLOCATION_H_

#include "base/basictypes.h"

namespace gpu {

// These are per context memory allocation limits set by the GpuMemoryManager
// and assigned to the browser and renderer context.
// They will change over time, given memory availability, and browser state.
struct MemoryAllocation {
  enum PriorityCutoff {
    // Allow no allocations.
    CUTOFF_ALLOW_NOTHING,
    // Allow only allocations that are strictly required for correct rendering.
    // For compositors, this is what is visible.
    CUTOFF_ALLOW_REQUIRED_ONLY,
    // Allow allocations that are not strictly needed for correct rendering, but
    // are nice to have for performance. For compositors, this includes textures
    // that are a few screens away from being visible.
    CUTOFF_ALLOW_NICE_TO_HAVE,
    // Allow all allocations.
    CUTOFF_ALLOW_EVERYTHING,
  };

  // Limits when this renderer is visible.
  uint64 bytes_limit_when_visible;
  PriorityCutoff priority_cutoff_when_visible;

  MemoryAllocation()
      : bytes_limit_when_visible(0),
        priority_cutoff_when_visible(CUTOFF_ALLOW_NOTHING) {
  }

  MemoryAllocation(uint64 bytes_limit_when_visible)
      : bytes_limit_when_visible(bytes_limit_when_visible),
        priority_cutoff_when_visible(CUTOFF_ALLOW_EVERYTHING) {
  }

  bool Equals(const MemoryAllocation& other) const {
    return bytes_limit_when_visible ==
               other.bytes_limit_when_visible &&
        priority_cutoff_when_visible == other.priority_cutoff_when_visible;
  }
};

// Memory Allocation request which is sent by a client, to help GpuMemoryManager
// more ideally split memory allocations across clients.
struct ManagedMemoryStats {
  // Bytes required for correct rendering.
  uint64 bytes_required;

  // Bytes that are not strictly required for correctness, but, if allocated,
  // will provide good performance.
  uint64 bytes_nice_to_have;

  // The number of bytes currently allocated.
  uint64 bytes_allocated;

  // Whether or not a backbuffer is currently requested (the memory usage
  // of the buffer is known by the GPU process).
  bool backbuffer_requested;

  ManagedMemoryStats()
      : bytes_required(0),
        bytes_nice_to_have(0),
        bytes_allocated(0),
        backbuffer_requested(false) {
  }

  ManagedMemoryStats(uint64 bytes_required,
                        uint64 bytes_nice_to_have,
                        uint64 bytes_allocated,
                        bool backbuffer_requested)
      : bytes_required(bytes_required),
        bytes_nice_to_have(bytes_nice_to_have),
        bytes_allocated(bytes_allocated),
        backbuffer_requested(backbuffer_requested) {
  }

  bool Equals(const ManagedMemoryStats& other) const {
    return bytes_required == other.bytes_required &&
        bytes_nice_to_have == other.bytes_nice_to_have &&
        bytes_allocated == other.bytes_allocated &&
        backbuffer_requested == other.backbuffer_requested;
  }
};

}  // namespace content

#endif // GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_ALLOCATION_H_
