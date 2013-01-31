// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_

#include "base/basictypes.h"

namespace content {

// These are per context memory allocation limits set by the GpuMemoryManager
// and assigned to the browser and renderer context.
// They will change over time, given memory availability, and browser state.

// Memory Allocation which will be assigned to the renderer context.
struct GpuMemoryAllocationForRenderer {
  enum PriorityCutoff {
    // Allow no allocations.
    kPriorityCutoffAllowNothing,
    // Allow only allocations that are strictly required for correct rendering.
    // For compositors, this is what is visible.
    kPriorityCutoffAllowOnlyRequired,
    // Allow allocations that are not strictly needed for correct rendering, but
    // are nice to have for performance. For compositors, this includes textures
    // that are a few screens away from being visible.
    kPriorityCutoffAllowNiceToHave,
    // Allow all allocations.
    kPriorityCutoffAllowEverything,
  };

  // Limits when this renderer is visible.
  uint64 bytes_limit_when_visible;
  PriorityCutoff priority_cutoff_when_visible;

  // Limits when this renderer is not visible.
  uint64 bytes_limit_when_not_visible;
  PriorityCutoff priority_cutoff_when_not_visible;
  bool have_backbuffer_when_not_visible;

  // If true, enforce this policy just once, but do not keep
  // it as a permanent policy.
  bool enforce_but_do_not_keep_as_policy;

  GpuMemoryAllocationForRenderer()
      : bytes_limit_when_visible(0),
        priority_cutoff_when_visible(kPriorityCutoffAllowNothing),
        bytes_limit_when_not_visible(0),
        priority_cutoff_when_not_visible(kPriorityCutoffAllowNothing),
        have_backbuffer_when_not_visible(false),
        enforce_but_do_not_keep_as_policy(false) {
  }

  GpuMemoryAllocationForRenderer(uint64 bytes_limit_when_visible)
      : bytes_limit_when_visible(bytes_limit_when_visible),
        priority_cutoff_when_visible(kPriorityCutoffAllowEverything),
        bytes_limit_when_not_visible(0),
        priority_cutoff_when_not_visible(kPriorityCutoffAllowNothing),
        have_backbuffer_when_not_visible(false),
        enforce_but_do_not_keep_as_policy(false) {
  }

  bool Equals(const GpuMemoryAllocationForRenderer& other) const {
    return bytes_limit_when_visible ==
               other.bytes_limit_when_visible &&
        priority_cutoff_when_visible == other.priority_cutoff_when_visible &&
        bytes_limit_when_not_visible == other.bytes_limit_when_not_visible &&
        priority_cutoff_when_not_visible ==
            other.priority_cutoff_when_not_visible &&
        have_backbuffer_when_not_visible ==
            other.have_backbuffer_when_not_visible &&
        enforce_but_do_not_keep_as_policy ==
            other.enforce_but_do_not_keep_as_policy;
  }
};

// Memory Allocation which will be assigned to the browser.
struct GpuMemoryAllocationForBrowser {
  bool suggest_have_frontbuffer;

  GpuMemoryAllocationForBrowser()
      : suggest_have_frontbuffer(false) {
  }

  GpuMemoryAllocationForBrowser(bool suggest_have_frontbuffer)
      : suggest_have_frontbuffer(suggest_have_frontbuffer) {
  }

  bool Equals(const GpuMemoryAllocationForBrowser& other) const {
      return suggest_have_frontbuffer == other.suggest_have_frontbuffer;
  }
};

// Combination of the above two Memory Allocations which will be created by the
// GpuMemoryManager.
struct GpuMemoryAllocation {
  GpuMemoryAllocationForRenderer renderer_allocation;
  GpuMemoryAllocationForBrowser browser_allocation;

  enum BufferAllocation {
    kHasNoFrontbuffer = 0,
    kHasFrontbuffer = 1,
  };

  GpuMemoryAllocation() {
  }

  GpuMemoryAllocation(uint64 gpu_resource_size_in_bytes,
                      BufferAllocation buffer_allocation)
      : renderer_allocation(gpu_resource_size_in_bytes),
        browser_allocation(buffer_allocation == kHasFrontbuffer) {
  }

  bool Equals(const GpuMemoryAllocation& other) const {
      return renderer_allocation.Equals(other.renderer_allocation) &&
          browser_allocation.Equals(other.browser_allocation);
  }
};

// Memory Allocation request which is sent by a client, to help GpuMemoryManager
// more ideally split memory allocations across clients.
struct GpuManagedMemoryStats {
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

  GpuManagedMemoryStats()
      : bytes_required(0),
        bytes_nice_to_have(0),
        bytes_allocated(0),
        backbuffer_requested(false) {
  }

  GpuManagedMemoryStats(uint64 bytes_required,
                        uint64 bytes_nice_to_have,
                        uint64 bytes_allocated,
                        bool backbuffer_requested)
      : bytes_required(bytes_required),
        bytes_nice_to_have(bytes_nice_to_have),
        bytes_allocated(bytes_allocated),
        backbuffer_requested(backbuffer_requested) {
  }

  bool Equals(const GpuManagedMemoryStats& other) const {
    return bytes_required == other.bytes_required &&
        bytes_nice_to_have == other.bytes_nice_to_have &&
        bytes_allocated == other.bytes_allocated &&
        backbuffer_requested == other.backbuffer_requested;
  }
};

}  // namespace content

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_
