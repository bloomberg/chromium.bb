// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_

#if defined(ENABLE_GPU)

#include <vector>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_allocation.h"

class GpuCommandBufferStubBase;

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

  // These are predefined values (in bytes) for
  // GpuMemoryAllocation::gpuResourceSizeInBytes.
  // Maximum Allocation for all tabs is a soft limit that can be exceeded
  // during the time it takes for renderers to respect new allocations,
  // including when switching tabs or opening a new window.
  // To alleviate some pressure, we decrease our desired limit by "one tabs'
  // worth" of memory.
  enum {
#if defined(OS_ANDROID)
    kMinimumAllocationForTab = 32 * 1024 * 1024,
    kMaximumAllocationForTabs = 64 * 1024 * 1024,
#else
    kMinimumAllocationForTab = 64 * 1024 * 1024,
    kMaximumAllocationForTabs = 512 * 1024 * 1024 - kMinimumAllocationForTab,
#endif
  };

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

  // Returns StubMemoryStat's for each GpuCommandBufferStubBase, which were
  // assigned during the most recent call to Manage().
  // Useful for tracking the memory-allocation-related presumed state of the
  // system, as seen by GpuMemoryManager.
  typedef base::hash_map<GpuCommandBufferStubBase*, StubMemoryStat>
      StubMemoryStatMap;
  const StubMemoryStatMap& stub_memory_stats_for_last_manage() const {
    return stub_memory_stats_for_last_manage_;
  }

  // Tries to estimate the total available gpu memory for use by
  // GpuMemoryManager.  Ideally should consider other system applications and
  // other internal but non GpuMemoryManager managed sources, etc.
  size_t GetAvailableGpuMemory() const;

  // GetPeakAssignedAllocationSum() will return the historical max value for the
  // sum of all assigned client allocations (ie, peak system memory allocation).
  size_t peak_assigned_allocation_sum() const {
    return peak_assigned_allocation_sum_;
  }

 private:
  friend class GpuMemoryManagerTest;
  void Manage();

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
  size_t peak_assigned_allocation_sum_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryManager);
};

#endif

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
