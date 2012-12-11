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
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/public/common/gpu_memory_stats.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "ui/gfx/size.h"

namespace content {
class GpuChannelManager;
class GpuMemoryManagerClient;
}

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<content::GpuMemoryManagerClient*> {
  size_t operator()(content::GpuMemoryManagerClient* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
} // namespace BASE_HASH_NAMESPACE
#endif // COMPILER

namespace content {
class GpuMemoryManagerClient;
class GpuMemoryTrackingGroup;

class CONTENT_EXPORT GpuMemoryManager :
    public base::SupportsWeakPtr<GpuMemoryManager> {
 public:
  enum { kDefaultMaxSurfacesWithFrontbufferSoftLimit = 8 };

  GpuMemoryManager(GpuChannelManager* channel_manager,
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
  void SetWindowCount(uint32 count);

  // Add and remove clients
  void AddClient(GpuMemoryManagerClient* client,
                 bool has_surface,
                 bool visible,
                 base::TimeTicks last_used_time);
  void RemoveClient(GpuMemoryManagerClient* client);
  void SetClientVisible(GpuMemoryManagerClient* client, bool visible);
  void SetClientManagedMemoryStats(GpuMemoryManagerClient* client,
                                   const GpuManagedMemoryStats& stats);

  // Add and remove structures to track context groups' memory consumption
  void AddTrackingGroup(GpuMemoryTrackingGroup* tracking_group);
  void RemoveTrackingGroup(GpuMemoryTrackingGroup* tracking_group);

  // Track a change in memory allocated by any context
  void TrackMemoryAllocatedChange(size_t old_size, size_t new_size);

 private:
  friend class GpuMemoryManagerTest;
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           ComparatorTests);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageBasicFunctionality);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageChangingVisibility);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageManyVisibleStubs);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageManyNotVisibleStubs);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageChangingLastUsedTime);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageChangingImportanceShareGroup);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestForegroundStubsGetBonusAllocation);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestUpdateAvailableGpuMemory);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           GpuMemoryAllocationCompareTests);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           StubMemoryStatsForLastManageTests);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManagedUsageTracking);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestBackgroundCutoff);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestBackgroundMru);

  struct ClientState {
    ClientState(GpuMemoryManagerClient* client,
                bool has_surface,
                bool visible,
                base::TimeTicks last_used_time);
    // The client to send allocations to.
    GpuMemoryManagerClient* client;

    // Offscreen commandbuffers will not have a surface.
    bool has_surface;

    // The last used time is determined by the last time that visibility
    // was changed.
    bool visible;
    base::TimeTicks last_used_time;

    // Statistics about memory usage.
    GpuManagedMemoryStats managed_memory_stats;

    // Set to disable allocating a frontbuffer or to disable allocations
    // for clients that don't have surfaces.
    bool hibernated;
  };

  class CONTENT_EXPORT ClientsComparator {
   public:
    bool operator()(ClientState* lhs,
                    ClientState* rhs);
  };

  typedef std::map<GpuMemoryManagerClient*, ClientState*> ClientMap;

  typedef std::vector<ClientState*> ClientStateVector;

  void Manage();
  void SetClientsHibernatedState(const ClientStateVector& clients) const;
  size_t GetVisibleClientAllocation(const ClientStateVector& clients) const;
  size_t GetCurrentBackgroundedAvailableGpuMemory() const;

  // Update the amount of GPU memory we think we have in the system, based
  // on what the stubs' contexts report.
  void UpdateAvailableGpuMemory(const ClientStateVector& clients);
  void UpdateBackgroundedAvailableGpuMemory();

  // The amount of video memory which is available for allocation.
  size_t GetAvailableGpuMemory() const;

  // Minimum value of available GPU memory, no matter how little the GPU
  // reports. This is the default value.
  size_t GetDefaultAvailableGpuMemory() const;

  // Maximum cap on total GPU memory, no matter how much the GPU reports.
  size_t GetMaximumTotalGpuMemory() const;

  // The maximum and minimum amount of memory that a tab may be assigned.
  size_t GetMaximumTabAllocation() const;
  size_t GetMinimumTabAllocation() const;

  // Get a reasonable memory limit from a viewport's surface area.
  static size_t CalcAvailableFromViewportArea(int viewport_area);
  static size_t CalcAvailableFromGpuTotal(size_t total_gpu_memory);

  // Send memory usage stats to the browser process.
  void SendUmaStatsToBrowser();

  // Interfaces for testing
  void TestingSetClientVisible(GpuMemoryManagerClient* client, bool visible);
  void TestingSetClientLastUsedTime(GpuMemoryManagerClient* client,
                                    base::TimeTicks last_used_time);
  void TestingSetClientHasSurface(GpuMemoryManagerClient* client,
                                  bool has_surface);
  bool TestingCompareClients(GpuMemoryManagerClient* lhs,
                             GpuMemoryManagerClient* rhs) const;
  void TestingDisableScheduleManage() { disable_schedule_manage_ = true; }
  void TestingSetAvailableGpuMemory(size_t bytes) {
    bytes_available_gpu_memory_ = bytes;
    bytes_available_gpu_memory_overridden_ = true;
  }

  void TestingSetBackgroundedAvailableGpuMemory(size_t bytes) {
    bytes_backgrounded_available_gpu_memory_ = bytes;
  }

  GpuChannelManager* channel_manager_;

  // All clients of this memory manager which have callbacks we
  // can use to adjust memory usage
  ClientMap clients_;

  // All context groups' tracking structures
  std::set<GpuMemoryTrackingGroup*> tracking_groups_;

  base::CancelableClosure delayed_manage_callback_;
  bool manage_immediate_scheduled_;

  size_t max_surfaces_with_frontbuffer_soft_limit_;

  // The maximum amount of memory that may be allocated for GPU resources
  size_t bytes_available_gpu_memory_;
  bool bytes_available_gpu_memory_overridden_;

  // The maximum amount of memory that can be allocated for GPU resources
  // in backgrounded renderers.
  size_t bytes_backgrounded_available_gpu_memory_;

  // The current total memory usage, and historical maximum memory usage
  size_t bytes_allocated_current_;
  size_t bytes_allocated_managed_visible_;
  size_t bytes_allocated_managed_backgrounded_;
  size_t bytes_allocated_historical_max_;

  // The number of browser windows that exist. If we ever receive a
  // GpuMsg_SetVideoMemoryWindowCount, then we use this to compute memory
  // budgets, instead of doing more complicated stub-based calculations.
  bool window_count_has_been_received_;
  uint32 window_count_;

  // Used to disable automatic changes to Manage() in testing.
  bool disable_schedule_manage_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryManager);
};

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

}  // namespace content

#endif

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
