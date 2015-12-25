// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_

#include <stdint.h>

#include <list>
#include <map>

#include "base/cancelable_callback.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/gpu_memory_stats.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/memory_tracking.h"

namespace content {

class GpuChannelManager;
class GpuMemoryTrackingGroup;

class CONTENT_EXPORT GpuMemoryManager :
    public base::SupportsWeakPtr<GpuMemoryManager> {
 public:
  explicit GpuMemoryManager(GpuChannelManager* channel_manager);
  ~GpuMemoryManager();

  // Retrieve GPU Resource consumption statistics for the task manager
  void GetVideoMemoryUsageStats(
      content::GPUVideoMemoryUsageStats* video_memory_usage_stats) const;

  GpuMemoryTrackingGroup* CreateTrackingGroup(
      base::ProcessId pid, gpu::gles2::MemoryTracker* memory_tracker);

  uint64_t GetTrackerMemoryUsage(gpu::gles2::MemoryTracker* tracker) const;

 private:
  friend class GpuMemoryManagerTest;
  friend class GpuMemoryTrackingGroup;
  friend class GpuMemoryManagerClientState;

  typedef std::map<gpu::gles2::MemoryTracker*, GpuMemoryTrackingGroup*>
      TrackingGroupMap;

  // Send memory usage stats to the browser process.
  void SendUmaStatsToBrowser();

  // Get the current number of bytes allocated.
  uint64_t GetCurrentUsage() const { return bytes_allocated_current_; }

  // GpuMemoryTrackingGroup interface
  void TrackMemoryAllocatedChange(GpuMemoryTrackingGroup* tracking_group,
                                  uint64_t old_size,
                                  uint64_t new_size);
  void OnDestroyTrackingGroup(GpuMemoryTrackingGroup* tracking_group);
  bool EnsureGPUMemoryAvailable(uint64_t size_needed);

  GpuChannelManager* channel_manager_;

  // All context groups' tracking structures
  TrackingGroupMap tracking_groups_;

  // The current total memory usage, and historical maximum memory usage
  uint64_t bytes_allocated_current_;
  uint64_t bytes_allocated_historical_max_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryManager);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
