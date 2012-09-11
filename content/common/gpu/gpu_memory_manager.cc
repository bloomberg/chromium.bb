// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_manager.h"

#if defined(ENABLE_GPU)

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "gpu/command_buffer/service/gpu_switches.h"

namespace {

const int kDelayedScheduleManageTimeoutMs = 67;

bool IsInSameContextShareGroupAsAnyOf(
    const GpuCommandBufferStubBase* stub,
    const std::vector<GpuCommandBufferStubBase*>& stubs) {
  for (std::vector<GpuCommandBufferStubBase*>::const_iterator it =
      stubs.begin(); it != stubs.end(); ++it) {
    if (stub->IsInSameContextShareGroup(**it))
      return true;
  }
  return false;
}

void AssignMemoryAllocations(
    GpuMemoryManager::StubMemoryStatMap* stub_memory_stats,
    const std::vector<GpuCommandBufferStubBase*>& stubs,
    GpuMemoryAllocation allocation,
    bool visible) {
  for (std::vector<GpuCommandBufferStubBase*>::const_iterator it =
          stubs.begin();
      it != stubs.end();
      ++it) {
    (*it)->SetMemoryAllocation(allocation);
    (*stub_memory_stats)[*it].allocation = allocation;
    (*stub_memory_stats)[*it].visible = visible;
  }
}

}

size_t GpuMemoryManager::CalculateBonusMemoryAllocationBasedOnSize(
    gfx::Size size) const {
  const int kViewportMultiplier = 16;
  const unsigned int kComponentsPerPixel = 4; // GraphicsContext3D::RGBA
  const unsigned int kBytesPerComponent = 1; // sizeof(GC3Dubyte)

  if (size.IsEmpty())
    return 0;

  size_t limit = kViewportMultiplier * size.width() * size.height() *
                 kComponentsPerPixel * kBytesPerComponent;
  if (limit < GetMinimumTabAllocation())
    limit = GetMinimumTabAllocation();
  else if (limit > GetAvailableGpuMemory())
    limit = GetAvailableGpuMemory();
  return limit - GetMinimumTabAllocation();
}

GpuMemoryManager::GpuMemoryManager(GpuMemoryManagerClient* client,
        size_t max_surfaces_with_frontbuffer_soft_limit)
    : client_(client),
      manage_immediate_scheduled_(false),
      max_surfaces_with_frontbuffer_soft_limit_(
          max_surfaces_with_frontbuffer_soft_limit),
      bytes_available_gpu_memory_(0),
      bytes_allocated_current_(0),
      bytes_allocated_historical_max_(0) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceGpuMemAvailableMb)) {
    base::StringToSizeT(
      command_line->GetSwitchValueASCII(switches::kForceGpuMemAvailableMb),
      &bytes_available_gpu_memory_);
    bytes_available_gpu_memory_ *= 1024 * 1024;
  } else {
#if defined(OS_ANDROID)
    bytes_available_gpu_memory_ = 64 * 1024 * 1024;
#else
#if defined(OS_CHROMEOS)
    bytes_available_gpu_memory_ = 1024 * 1024 * 1024;
#else
    bytes_available_gpu_memory_ = 256 * 1024 * 1024;
#endif
#endif
  }
}

GpuMemoryManager::~GpuMemoryManager() {
  DCHECK(tracking_groups_.empty());
}

bool GpuMemoryManager::StubWithSurfaceComparator::operator()(
    GpuCommandBufferStubBase* lhs,
    GpuCommandBufferStubBase* rhs) {
  DCHECK(lhs->has_surface_state() && rhs->has_surface_state());
  const GpuCommandBufferStubBase::SurfaceState& lhs_ss = lhs->surface_state();
  const GpuCommandBufferStubBase::SurfaceState& rhs_ss = rhs->surface_state();
  if (lhs_ss.visible)
    return !rhs_ss.visible || (lhs_ss.last_used_time > rhs_ss.last_used_time);
  else
    return !rhs_ss.visible && (lhs_ss.last_used_time > rhs_ss.last_used_time);
};

void GpuMemoryManager::ScheduleManage(bool immediate) {
  if (manage_immediate_scheduled_)
    return;
  if (immediate) {
    MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GpuMemoryManager::Manage, AsWeakPtr()));
    manage_immediate_scheduled_ = true;
    if (!delayed_manage_callback_.IsCancelled())
      delayed_manage_callback_.Cancel();
  } else {
    if (!delayed_manage_callback_.IsCancelled())
      return;
    delayed_manage_callback_.Reset(base::Bind(&GpuMemoryManager::Manage,
                                              AsWeakPtr()));
    MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      delayed_manage_callback_.callback(),
      base::TimeDelta::FromMilliseconds(kDelayedScheduleManageTimeoutMs));
  }
}

void GpuMemoryManager::TrackMemoryAllocatedChange(size_t old_size,
                                                  size_t new_size) {
  if (new_size < old_size) {
    size_t delta = old_size - new_size;
    DCHECK(bytes_allocated_current_ >= delta);
    bytes_allocated_current_ -= delta;
  } else {
    size_t delta = new_size - old_size;
    bytes_allocated_current_ += delta;
    if (bytes_allocated_current_ > bytes_allocated_historical_max_) {
      bytes_allocated_historical_max_ = bytes_allocated_current_;
    }
  }
  if (new_size != old_size) {
    TRACE_COUNTER_ID1("GpuMemoryManager",
                      "GpuMemoryUsage",
                      this,
                      bytes_allocated_current_);
  }
}

void GpuMemoryManager::AddTrackingGroup(
    GpuMemoryTrackingGroup* tracking_group) {
  tracking_groups_.insert(tracking_group);
}

void GpuMemoryManager::RemoveTrackingGroup(
    GpuMemoryTrackingGroup* tracking_group) {
  tracking_groups_.erase(tracking_group);
}

void GpuMemoryManager::GetVideoMemoryUsageStats(
    content::GPUVideoMemoryUsageStats& video_memory_usage_stats) const {
  // For each context group, assign its memory usage to its PID
  video_memory_usage_stats.process_map.clear();
  for (std::set<GpuMemoryTrackingGroup*>::const_iterator i =
       tracking_groups_.begin(); i != tracking_groups_.end(); ++i) {
    const GpuMemoryTrackingGroup* tracking_group = (*i);
    video_memory_usage_stats.process_map[
        tracking_group->GetPid()].video_memory += tracking_group->GetSize();
  }

  // Assign the total across all processes in the GPU process
  video_memory_usage_stats.process_map[
      base::GetCurrentProcId()].video_memory = bytes_allocated_current_;
  video_memory_usage_stats.process_map[
      base::GetCurrentProcId()].has_duplicates = true;
}

// The current Manage algorithm simply classifies contexts (stubs) into
// "foreground", "background", or "hibernated" categories.
// For each of these three categories, there are predefined memory allocation
// limits and front/backbuffer states.
//
// Stubs may or may not have a surfaces, and the rules are different for each.
//
// The rules for categorizing contexts with a surface are:
//  1. Foreground: All visible surfaces.
//                 * Must have both front and back buffer.
//
//  2. Background: Non visible surfaces, which have not surpassed the
//                 max_surfaces_with_frontbuffer_soft_limit_ limit.
//                 * Will have only a frontbuffer.
//
//  3. Hibernated: Non visible surfaces, which have surpassed the
//                 max_surfaces_with_frontbuffer_soft_limit_ limit.
//                 * Will not have either buffer.
//
// The considerations for categorizing contexts without a surface are:
//  1. These contexts do not track {visibility,last_used_time}, so cannot
//     sort them directly.
//  2. These contexts may be used by, and thus affect, other contexts, and so
//     cannot be less visible than any affected context.
//  3. Contexts belong to share groups within which resources can be shared.
//
// As such, the rule for categorizing contexts without a surface is:
//  1. Find the most visible context-with-a-surface within each
//     context-without-a-surface's share group, and inherit its visibilty.
void GpuMemoryManager::Manage() {
  manage_immediate_scheduled_ = false;
  delayed_manage_callback_.Cancel();

  // Create stub lists by separating out the two types received from client
  std::vector<GpuCommandBufferStubBase*> stubs_with_surface;
  std::vector<GpuCommandBufferStubBase*> stubs_without_surface;
  {
    std::vector<GpuCommandBufferStubBase*> stubs;
    client_->AppendAllCommandBufferStubs(stubs);

    for (std::vector<GpuCommandBufferStubBase*>::iterator it = stubs.begin();
        it != stubs.end(); ++it) {
      GpuCommandBufferStubBase* stub = *it;
      if (!stub->client_has_memory_allocation_changed_callback())
        continue;
      if (stub->has_surface_state())
        stubs_with_surface.push_back(stub);
      else
        stubs_without_surface.push_back(stub);
    }
  }

  // Sort stubs with surface into {visibility,last_used_time} order using
  // custom comparator
  std::sort(stubs_with_surface.begin(),
            stubs_with_surface.end(),
            StubWithSurfaceComparator());
  DCHECK(std::unique(stubs_with_surface.begin(), stubs_with_surface.end()) ==
         stubs_with_surface.end());

  // Separate stubs into memory allocation sets.
  std::vector<GpuCommandBufferStubBase*> stubs_with_surface_foreground,
                                         stubs_with_surface_background,
                                         stubs_with_surface_hibernated,
                                         stubs_without_surface_foreground,
                                         stubs_without_surface_background,
                                         stubs_without_surface_hibernated;

  for (size_t i = 0; i < stubs_with_surface.size(); ++i) {
    GpuCommandBufferStubBase* stub = stubs_with_surface[i];
    DCHECK(stub->has_surface_state());
    if (stub->surface_state().visible)
      stubs_with_surface_foreground.push_back(stub);
    else if (i < max_surfaces_with_frontbuffer_soft_limit_)
      stubs_with_surface_background.push_back(stub);
    else
      stubs_with_surface_hibernated.push_back(stub);
  }
  for (std::vector<GpuCommandBufferStubBase*>::const_iterator it =
      stubs_without_surface.begin(); it != stubs_without_surface.end(); ++it) {
    GpuCommandBufferStubBase* stub = *it;
    DCHECK(!stub->has_surface_state());

    // Stubs without surfaces have deduced allocation state using the state
    // of surface stubs which are in the same context share group.
    if (IsInSameContextShareGroupAsAnyOf(stub, stubs_with_surface_foreground))
      stubs_without_surface_foreground.push_back(stub);
    else if (IsInSameContextShareGroupAsAnyOf(
        stub, stubs_with_surface_background))
      stubs_without_surface_background.push_back(stub);
    else
      stubs_without_surface_hibernated.push_back(stub);
  }

  size_t bonus_allocation = 0;
#if !defined(OS_ANDROID)
  // Calculate bonus allocation by splitting remainder of global limit equally
  // after giving out the minimum to those that need it.
  size_t num_stubs_need_mem = stubs_with_surface_foreground.size() +
                              stubs_without_surface_foreground.size() +
                              stubs_without_surface_background.size();
  size_t base_allocation_size = GetMinimumTabAllocation() * num_stubs_need_mem;
  if (base_allocation_size < GetAvailableGpuMemory() &&
      !stubs_with_surface_foreground.empty())
    bonus_allocation = (GetAvailableGpuMemory() - base_allocation_size) /
                       stubs_with_surface_foreground.size();
#else
  // On android, calculate bonus allocation based on surface size.
  if (!stubs_with_surface_foreground.empty())
    bonus_allocation = CalculateBonusMemoryAllocationBasedOnSize(
        stubs_with_surface_foreground[0]->GetSurfaceSize());
#endif
  size_t stubs_with_surface_foreground_allocation = GetMinimumTabAllocation() +
                                                    bonus_allocation;
  if (stubs_with_surface_foreground_allocation >= GetMaximumTabAllocation())
    stubs_with_surface_foreground_allocation = GetMaximumTabAllocation();

  stub_memory_stats_for_last_manage_.clear();

  // Now give out allocations to everyone.
  AssignMemoryAllocations(
      &stub_memory_stats_for_last_manage_,
      stubs_with_surface_foreground,
      GpuMemoryAllocation(stubs_with_surface_foreground_allocation,
          GpuMemoryAllocation::kHasFrontbuffer |
          GpuMemoryAllocation::kHasBackbuffer),
      true);

  AssignMemoryAllocations(
      &stub_memory_stats_for_last_manage_,
      stubs_with_surface_background,
      GpuMemoryAllocation(0, GpuMemoryAllocation::kHasFrontbuffer),
      false);

  AssignMemoryAllocations(
      &stub_memory_stats_for_last_manage_,
      stubs_with_surface_hibernated,
      GpuMemoryAllocation(0, GpuMemoryAllocation::kHasNoBuffers),
      false);

  AssignMemoryAllocations(
      &stub_memory_stats_for_last_manage_,
      stubs_without_surface_foreground,
      GpuMemoryAllocation(GetMinimumTabAllocation(),
          GpuMemoryAllocation::kHasNoBuffers),
      true);

  AssignMemoryAllocations(
      &stub_memory_stats_for_last_manage_,
      stubs_without_surface_background,
      GpuMemoryAllocation(GetMinimumTabAllocation(),
          GpuMemoryAllocation::kHasNoBuffers),
      false);

  AssignMemoryAllocations(
      &stub_memory_stats_for_last_manage_,
      stubs_without_surface_hibernated,
      GpuMemoryAllocation(0, GpuMemoryAllocation::kHasNoBuffers),
      false);
}

#endif
