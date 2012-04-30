// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_
#pragma once

#include "base/basictypes.h"

// These are per context memory allocation limits set by the GpuMemoryManager
// and assigned to the browser and renderer context.
// They will change over time, given memory availability, and browser state.

// Memory Allocation which will be assigned to the renderer context.
struct GpuMemoryAllocationForRenderer {
  enum {
    INVALID_RESOURCE_SIZE = -1
  };

  // Exceeding this limit for an unreasonable amount of time may cause context
  // to be lost.
  size_t gpu_resource_size_in_bytes;
  bool suggest_have_backbuffer;

  GpuMemoryAllocationForRenderer()
      : gpu_resource_size_in_bytes(0),
        suggest_have_backbuffer(false) {
  }

  GpuMemoryAllocationForRenderer(size_t gpu_resource_size_in_bytes,
                                 bool suggest_have_backbuffer)
      : gpu_resource_size_in_bytes(gpu_resource_size_in_bytes),
        suggest_have_backbuffer(suggest_have_backbuffer) {
  }

  bool operator==(const GpuMemoryAllocationForRenderer& other) const {
    return gpu_resource_size_in_bytes == other.gpu_resource_size_in_bytes &&
           suggest_have_backbuffer == other.suggest_have_backbuffer;
  }
  bool operator!=(const GpuMemoryAllocationForRenderer& other) const {
    return !(*this == other);
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

  bool operator==(const GpuMemoryAllocationForBrowser& other) const {
      return suggest_have_frontbuffer == other.suggest_have_frontbuffer;
  }
  bool operator!=(const GpuMemoryAllocationForBrowser& other) const {
    return !(*this == other);
  }
};

// Combination of the above two Memory Allocations which will be created by the
// GpuMemoryManager.
struct GpuMemoryAllocation : public GpuMemoryAllocationForRenderer,
                             public GpuMemoryAllocationForBrowser {
  // Bitmap
  enum BufferAllocation {
    kHasNoBuffers = 0,
    kHasFrontbuffer = 1,
    kHasBackbuffer = 2
  };

  GpuMemoryAllocation()
      : GpuMemoryAllocationForRenderer(),
        GpuMemoryAllocationForBrowser() {
  }

  GpuMemoryAllocation(size_t gpu_resource_size_in_bytes,
                      int allocationBitmap)
      : GpuMemoryAllocationForRenderer(gpu_resource_size_in_bytes,
            (allocationBitmap & kHasBackbuffer) == kHasBackbuffer),
        GpuMemoryAllocationForBrowser(
            (allocationBitmap & kHasFrontbuffer) == kHasFrontbuffer) {
  }

  bool operator==(const GpuMemoryAllocation& other) const {
      return static_cast<const GpuMemoryAllocationForRenderer&>(*this) ==
             static_cast<const GpuMemoryAllocationForRenderer&>(other) &&
             static_cast<const GpuMemoryAllocationForBrowser&>(*this) ==
             static_cast<const GpuMemoryAllocationForBrowser&>(other);
  }
  bool operator!=(const GpuMemoryAllocation& other) const {
    return !(*this == other);
  }
};

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_ALLOCATION_H_
