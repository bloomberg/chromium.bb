// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_CLIENT_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_CLIENT_H_

#if defined(ENABLE_GPU)

#include <list>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "ui/gfx/size.h"

namespace content {

class GpuMemoryManager;
class GpuMemoryTrackingGroup;

// The interface that the GPU memory manager uses to manipulate a client (to
// send it allocation information and query its properties).
class CONTENT_EXPORT GpuMemoryManagerClient {
 public:
  virtual ~GpuMemoryManagerClient() {}

  // Returns surface size.
  virtual gfx::Size GetSurfaceSize() const = 0;

  // Returns the memory tracker for this stub.
  virtual gpu::gles2::MemoryTracker* GetMemoryTracker() const = 0;

  // Sets buffer usage depending on Memory Allocation
  virtual void SetMemoryAllocation(
      const GpuMemoryAllocation& allocation) = 0;

  // Returns in bytes the total amount of GPU memory for the GPU which this
  // context is currently rendering on. Returns false if no extension exists
  // to get the exact amount of GPU memory.
  virtual bool GetTotalGpuMemory(size_t* bytes) = 0;
};

// The state associated with a GPU memory manager client. This acts as the
// handle through which the client interacts with the GPU memory manager.
class CONTENT_EXPORT GpuMemoryManagerClientState {
 public:
  ~GpuMemoryManagerClientState();
  void SetVisible(bool visible);
  void SetManagedMemoryStats(const GpuManagedMemoryStats& stats);

 private:
  friend class GpuMemoryManager;

  GpuMemoryManagerClientState(GpuMemoryManager* memory_manager,
                              GpuMemoryManagerClient* client,
                              GpuMemoryTrackingGroup* tracking_group,
                              bool has_surface,
                              bool visible);

  // The memory manager this client is hanging off of.
  GpuMemoryManager* memory_manager_;

  // The client to send allocations to.
  GpuMemoryManagerClient* client_;

  // The tracking group for this client.
  GpuMemoryTrackingGroup* tracking_group_;

  // Offscreen commandbuffers will not have a surface.
  const bool has_surface_;

  // Whether or not this client is visible.
  bool visible_;

  // If the client has a surface, then this is an iterator in the
  // clients_visible_mru_ if this client is visible and
  // clients_nonvisible_mru_ if this is non-visible. Otherwise this is an
  // iterator in clients_nonsurface_.
  std::list<GpuMemoryManagerClientState*>::iterator list_iterator_;
  bool list_iterator_valid_;

  // Statistics about memory usage.
  GpuManagedMemoryStats managed_memory_stats_;

  // When managed_memory_stats_.bytes_nicetohave leaves the range
  // [low_, high_], then re-adjust memory limits.
  size_t bytes_nicetohave_limit_low_;
  size_t bytes_nicetohave_limit_high_;

  // The allocation for this client, used transiently during memory policy
  // calculation.
  size_t bytes_allocation_when_visible_;
  size_t bytes_allocation_when_nonvisible_;

  // The ideal allocation for this client for three performance levels, used
  // transiently during memory policy calculation.
  size_t bytes_allocation_ideal_nicetohave_;
  size_t bytes_allocation_ideal_required_;
  size_t bytes_allocation_ideal_minimum_;

  // Set to disable allocating a frontbuffer or to disable allocations
  // for clients that don't have surfaces.
  bool hibernated_;
};

}  // namespace content

#endif

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_CLIENT_H_
