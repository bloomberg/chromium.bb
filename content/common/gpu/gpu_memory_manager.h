// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_

#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/public/common/gpu_memory_stats.h"
#include "gpu/command_buffer/service/memory_tracking.h"

namespace content {

class GpuChannelManager;
class GpuMemoryManagerClient;
class GpuMemoryManagerClientState;
class GpuMemoryTrackingGroup;

class CONTENT_EXPORT GpuMemoryManager :
    public base::SupportsWeakPtr<GpuMemoryManager> {
 public:
  enum { kDefaultMaxSurfacesWithFrontbufferSoftLimit = 8 };
  enum ScheduleManageTime {
    // Add a call to Manage to the thread's message loop immediately.
    kScheduleManageNow,
    // Add a Manage call to the thread's message loop for execution 1/60th of
    // of a second from now.
    kScheduleManageLater,
  };

  GpuMemoryManager(GpuChannelManager* channel_manager,
                   size_t max_surfaces_with_frontbuffer_soft_limit);
  ~GpuMemoryManager();

  // Schedule a Manage() call. If immediate is true, we PostTask without delay.
  // Otherwise PostDelayedTask using a CancelableClosure and allow multiple
  // delayed calls to "queue" up. This way, we do not spam clients in certain
  // lower priority situations. An immediate schedule manage will cancel any
  // queued delayed manage.
  void ScheduleManage(ScheduleManageTime schedule_manage_time);

  // Retrieve GPU Resource consumption statistics for the task manager
  void GetVideoMemoryUsageStats(
      content::GPUVideoMemoryUsageStats* video_memory_usage_stats) const;
  void SetWindowCount(uint32 count);

  GpuMemoryManagerClientState* CreateClientState(
      GpuMemoryManagerClient* client, bool has_surface, bool visible);

  GpuMemoryTrackingGroup* CreateTrackingGroup(
      base::ProcessId pid, gpu::gles2::MemoryTracker* memory_tracker);

 private:
  friend class GpuMemoryManagerTest;
  friend class GpuMemoryTrackingGroup;
  friend class GpuMemoryManagerClientState;

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
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestUnmanagedTracking);

  typedef std::map<gpu::gles2::MemoryTracker*, GpuMemoryTrackingGroup*>
      TrackingGroupMap;

  typedef std::list<GpuMemoryManagerClientState*> ClientStateList;

  void Manage();
  void SetClientsHibernatedState() const;
  size_t GetVisibleClientAllocation() const;
  size_t GetCurrentNonvisibleAvailableGpuMemory() const;
  void AssignSurfacesAllocationsNonuniform();
  void AssignSurfacesAllocationsUniform();
  void AssignNonSurfacesAllocations();

  // Update the amount of GPU memory we think we have in the system, based
  // on what the stubs' contexts report.
  void UpdateAvailableGpuMemory();
  void UpdateUnmanagedMemoryLimits();
  void UpdateNonvisibleAvailableGpuMemory();

  // The amount of video memory which is available for allocation.
  size_t GetAvailableGpuMemory() const;

  // Minimum value of available GPU memory, no matter how little the GPU
  // reports. This is the default value.
  size_t GetDefaultAvailableGpuMemory() const;

  // Maximum cap on total GPU memory, no matter how much the GPU reports.
  size_t GetMaximumTotalGpuMemory() const;

  // The maximum and minimum amount of memory that a tab may be assigned.
  size_t GetMaximumClientAllocation() const;
  size_t GetMinimumClientAllocation() const;

  // Get a reasonable memory limit from a viewport's surface area.
  static size_t CalcAvailableFromViewportArea(int viewport_area);
  static size_t CalcAvailableFromGpuTotal(size_t total_gpu_memory);

  // Send memory usage stats to the browser process.
  void SendUmaStatsToBrowser();

  // Get the current number of bytes allocated.
  size_t GetCurrentUsage() const {
    return bytes_allocated_managed_current_ +
        bytes_allocated_unmanaged_current_;
  }

  // GpuMemoryTrackingGroup interface
  void TrackMemoryAllocatedChange(
      GpuMemoryTrackingGroup* tracking_group,
      size_t old_size,
      size_t new_size,
      gpu::gles2::MemoryTracker::Pool tracking_pool);
  void OnDestroyTrackingGroup(GpuMemoryTrackingGroup* tracking_group);

  // GpuMemoryManagerClientState interface
  void SetClientStateVisible(
      GpuMemoryManagerClientState* client_state, bool visible);
  void SetClientStateManagedMemoryStats(
      GpuMemoryManagerClientState* client_state,
      const GpuManagedMemoryStats& stats);
  void OnDestroyClientState(GpuMemoryManagerClientState* client);

  // Add or remove a client from its clients list (visible, nonvisible, or
  // nonsurface). When adding the client, add it to the front of the list.
  void AddClientToList(GpuMemoryManagerClientState* client_state);
  void RemoveClientFromList(GpuMemoryManagerClientState* client_state);
  ClientStateList* GetClientList(GpuMemoryManagerClientState* client_state);

  // Interfaces for testing
  void TestingDisableScheduleManage() { disable_schedule_manage_ = true; }
  void TestingSetAvailableGpuMemory(size_t bytes) {
    bytes_available_gpu_memory_ = bytes;
    bytes_available_gpu_memory_overridden_ = true;
  }

  void TestingSetMinimumClientAllocation(size_t bytes) {
    bytes_minimum_per_client_ = bytes;
    bytes_minimum_per_client_overridden_ = true;
  }

  void TestingSetUnmanagedLimitStep(size_t bytes) {
    bytes_unmanaged_limit_step_ = bytes;
  }

  void TestingSetNonvisibleAvailableGpuMemory(size_t bytes) {
    bytes_nonvisible_available_gpu_memory_ = bytes;
  }

  GpuChannelManager* channel_manager_;

  // The new memory policy does not uniformly assign memory to tabs, but
  // scales the assignments to the tabs' needs.
  bool use_nonuniform_memory_policy_;

  // A list of all visible and nonvisible clients, in most-recently-used
  // order (most recently used is first).
  ClientStateList clients_visible_mru_;
  ClientStateList clients_nonvisible_mru_;

  // A list of all clients that don't have a surface.
  ClientStateList clients_nonsurface_;

  // All context groups' tracking structures
  TrackingGroupMap tracking_groups_;

  base::CancelableClosure delayed_manage_callback_;
  bool manage_immediate_scheduled_;

  size_t max_surfaces_with_frontbuffer_soft_limit_;

  // The maximum amount of memory that may be allocated for GPU resources
  size_t bytes_available_gpu_memory_;
  bool bytes_available_gpu_memory_overridden_;

  // The minimum allocation that may be given to a single renderer.
  size_t bytes_minimum_per_client_;
  bool bytes_minimum_per_client_overridden_;

  // The maximum amount of memory that can be allocated for GPU resources
  // in nonvisible renderers.
  size_t bytes_nonvisible_available_gpu_memory_;

  // The current total memory usage, and historical maximum memory usage
  size_t bytes_allocated_managed_current_;
  size_t bytes_allocated_managed_visible_;
  size_t bytes_allocated_managed_nonvisible_;
  size_t bytes_allocated_unmanaged_current_;
  size_t bytes_allocated_historical_max_;

  // If bytes_allocated_unmanaged_current_ leaves the interval [low_, high_),
  // then ScheduleManage to take the change into account.
  size_t bytes_allocated_unmanaged_high_;
  size_t bytes_allocated_unmanaged_low_;

  // Update bytes_allocated_unmanaged_low/high_ in intervals of step_.
  size_t bytes_unmanaged_limit_step_;

  // The number of browser windows that exist. If we ever receive a
  // GpuMsg_SetVideoMemoryWindowCount, then we use this to compute memory
  // allocations, instead of doing more complicated stub-based calculations.
  bool window_count_has_been_received_;
  uint32 window_count_;

  // Used to disable automatic changes to Manage() in testing.
  bool disable_schedule_manage_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryManager);
};

}  // namespace content

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
