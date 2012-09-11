// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_

#if defined(ENABLE_GPU)

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/public/common/gpu_memory_stats.h"
#include "ui/gfx/size.h"

class GpuCommandBufferStubBase;
class GpuMemoryTrackingGroup;

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<GpuCommandBufferStubBase*> {
  size_t operator()(GpuCommandBufferStubBase* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
} // namespace BASE_HASH_NAMESPACE
#endif // COMPILER

class CONTENT_EXPORT GpuMemoryManagerClient {
public:
  virtual ~GpuMemoryManagerClient() {}

  virtual void AppendAllCommandBufferStubs(
      std::vector<GpuCommandBufferStubBase*>& stubs) = 0;
};

class CONTENT_EXPORT GpuMemoryManager :
    public base::SupportsWeakPtr<GpuMemoryManager> {
 public:
  enum { kDefaultMaxSurfacesWithFrontbufferSoftLimit = 8 };


  // StubMemoryStat is used to store memory-allocation-related information about
  // a GpuCommandBufferStubBase for some time point.
  struct StubMemoryStat {
    bool visible;
    GpuMemoryAllocationRequest requested_allocation;
    GpuMemoryAllocation allocation;
  };

  GpuMemoryManager(GpuMemoryManagerClient* client,
                   size_t max_surfaces_with_frontbuffer_soft_limit);
  ~GpuMemoryManager();

  // Schedule a Manage() call. If immediate is true, we PostTask without delay.
  // Otherwise PostDelayedTask using a CancelableClosure and allow multiple
  // delayed calls to "queue" up. This way, we do not spam clients in certain
  // lower priority situations. An immediate schedule manage will cancel any
  // queued delayed manage.
  void ScheduleManage(bool immediate);

  // Retrieve GPU Resource consumption statistics for the task manager
  void GetVideoMemoryUsageStats(
      content::GPUVideoMemoryUsageStats& video_memory_usage_stats) const;

  // Add and remove structures to track context groups' memory consumption
  void AddTrackingGroup(GpuMemoryTrackingGroup* tracking_group);
  void RemoveTrackingGroup(GpuMemoryTrackingGroup* tracking_group);

  // Returns StubMemoryStat's for each GpuCommandBufferStubBase, which were
  // assigned during the most recent call to Manage().
  // Useful for tracking the memory-allocation-related presumed state of the
  // system, as seen by GpuMemoryManager.
  typedef base::hash_map<GpuCommandBufferStubBase*, StubMemoryStat>
      StubMemoryStatMap;
  const StubMemoryStatMap& stub_memory_stats_for_last_manage() const {
    return stub_memory_stats_for_last_manage_;
  }

  // Track a change in memory allocated by any context
  void TrackMemoryAllocatedChange(size_t old_size, size_t new_size);

 private:
  friend class GpuMemoryManagerTest;

  void Manage();

  // The context groups' tracking structures
  std::set<GpuMemoryTrackingGroup*> tracking_groups_;

  size_t CalculateBonusMemoryAllocationBasedOnSize(gfx::Size size) const;

  size_t GetAvailableGpuMemory() const {
    return bytes_available_gpu_memory_;
  }

  // The maximum amount of memory that a tab may be assigned
  size_t GetMaximumTabAllocation() const {
#if defined(OS_CHROMEOS)
    return bytes_available_gpu_memory_;
#else
    return 128 * 1024 * 1024;
#endif
  }

  // The minimum non-zero amount of memory that a tab may be assigned
  size_t GetMinimumTabAllocation() const {
#if defined(OS_ANDROID)
    return 32 * 1024 * 1024;
#else
    return 64 * 1024 * 1024;
#endif
  }

  class CONTENT_EXPORT StubWithSurfaceComparator {
   public:
    bool operator()(GpuCommandBufferStubBase* lhs,
                    GpuCommandBufferStubBase* rhs);
  };

  GpuMemoryManagerClient* client_;

  base::CancelableClosure delayed_manage_callback_;
  bool manage_immediate_scheduled_;

  size_t max_surfaces_with_frontbuffer_soft_limit_;

  StubMemoryStatMap stub_memory_stats_for_last_manage_;

  // The maximum amount of memory that may be allocated for GPU resources
  size_t bytes_available_gpu_memory_;

  // The current total memory usage, and historical maximum memory usage
  size_t bytes_allocated_current_;
  size_t bytes_allocated_historical_max_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryManager);
};

#endif

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
