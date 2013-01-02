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
#include "base/sys_info.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_manager_client.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "content/common/gpu/gpu_memory_uma_stats.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/service/gpu_switches.h"

namespace content {
namespace {

const int kDelayedScheduleManageTimeoutMs = 67;

const size_t kBytesAllocatedUnmanagedStep = 16 * 1024 * 1024;

void TrackValueChanged(size_t old_size, size_t new_size, size_t* total_size) {
  DCHECK(new_size > old_size || *total_size >= (old_size - new_size));
  *total_size += (new_size - old_size);
}

template<typename T>
T RoundUp(T n, T mul) {
  return ((n + mul - 1) / mul) * mul;
}

template<typename T>
T RoundDown(T n, T mul) {
  return (n / mul) * mul;
}

}

GpuMemoryManager::GpuMemoryManager(
    GpuChannelManager* channel_manager,
    size_t max_surfaces_with_frontbuffer_soft_limit)
    : channel_manager_(channel_manager),
      manage_immediate_scheduled_(false),
      max_surfaces_with_frontbuffer_soft_limit_(
          max_surfaces_with_frontbuffer_soft_limit),
      bytes_available_gpu_memory_(0),
      bytes_available_gpu_memory_overridden_(false),
      bytes_minimum_per_client_(0),
      bytes_minimum_per_client_overridden_(false),
      bytes_backgrounded_available_gpu_memory_(0),
      bytes_allocated_managed_current_(0),
      bytes_allocated_managed_visible_(0),
      bytes_allocated_managed_backgrounded_(0),
      bytes_allocated_unmanaged_current_(0),
      bytes_allocated_historical_max_(0),
      bytes_allocated_unmanaged_high_(0),
      bytes_allocated_unmanaged_low_(0),
      bytes_unmanaged_limit_step_(kBytesAllocatedUnmanagedStep),
      window_count_has_been_received_(false),
      window_count_(0),
      disable_schedule_manage_(false)
{
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceGpuMemAvailableMb)) {
    base::StringToSizeT(
      command_line->GetSwitchValueASCII(switches::kForceGpuMemAvailableMb),
      &bytes_available_gpu_memory_);
    bytes_available_gpu_memory_ *= 1024 * 1024;
    bytes_available_gpu_memory_overridden_ = true;
  } else
    bytes_available_gpu_memory_ = GetDefaultAvailableGpuMemory();
  UpdateBackgroundedAvailableGpuMemory();
}

GpuMemoryManager::~GpuMemoryManager() {
  DCHECK(tracking_groups_.empty());
  DCHECK(clients_visible_mru_.empty());
  DCHECK(clients_nonvisible_mru_.empty());
  DCHECK(clients_nonsurface_.empty());
  DCHECK(!bytes_allocated_managed_current_);
  DCHECK(!bytes_allocated_unmanaged_current_);
  DCHECK(!bytes_allocated_managed_visible_);
  DCHECK(!bytes_allocated_managed_backgrounded_);
}

size_t GpuMemoryManager::GetAvailableGpuMemory() const {
  // Allow unmanaged allocations to over-subscribe by at most (high_ - low_)
  // before restricting managed (compositor) memory based on unmanaged usage.
  if (bytes_allocated_unmanaged_low_ > bytes_available_gpu_memory_)
    return 0;
  return bytes_available_gpu_memory_ - bytes_allocated_unmanaged_low_;
}

size_t GpuMemoryManager::GetCurrentBackgroundedAvailableGpuMemory() const {
  if (bytes_allocated_managed_visible_ < GetAvailableGpuMemory()) {
    return std::min(bytes_backgrounded_available_gpu_memory_,
                    GetAvailableGpuMemory() - bytes_allocated_managed_visible_);
  }
  return 0;
}

size_t GpuMemoryManager::GetDefaultAvailableGpuMemory() const {
#if defined(OS_ANDROID)
  return 32 * 1024 * 1024;
#elif defined(OS_CHROMEOS)
  return 1024 * 1024 * 1024;
#else
  return 256 * 1024 * 1024;
#endif
}

size_t GpuMemoryManager::GetMaximumTotalGpuMemory() const {
#if defined(OS_ANDROID)
  return 256 * 1024 * 1024;
#else
  return 1024 * 1024 * 1024;
#endif
}

size_t GpuMemoryManager::GetMaximumTabAllocation() const {
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  return bytes_available_gpu_memory_;
#else
  // This is to avoid allowing a single page on to use a full 256MB of memory
  // (the current total limit). Long-scroll pages will hit this limit,
  // resulting in instability on some platforms (e.g, issue 141377).
  return bytes_available_gpu_memory_ / 2;
#endif
}

size_t GpuMemoryManager::GetMinimumTabAllocation() const {
  if (bytes_minimum_per_client_overridden_)
    return bytes_minimum_per_client_;
#if defined(OS_ANDROID)
  return 32 * 1024 * 1024;
#elif defined(OS_CHROMEOS)
  return 64 * 1024 * 1024;
#else
  return 64 * 1024 * 1024;
#endif
}

size_t GpuMemoryManager::CalcAvailableFromViewportArea(int viewport_area) {
  // We can't query available GPU memory from the system on Android, but
  // 18X the viewport and 50% of the dalvik heap size give us a good
  // estimate of available GPU memory on a wide range of devices.
  const int kViewportMultiplier = 18;
  const unsigned int kComponentsPerPixel = 4; // GraphicsContext3D::RGBA
  const unsigned int kBytesPerComponent = 1; // sizeof(GC3Dubyte)
  size_t viewport_limit = viewport_area * kViewportMultiplier *
                                          kComponentsPerPixel *
                                          kBytesPerComponent;
#if !defined(OS_ANDROID)
  return viewport_limit;
#else
  static size_t dalvik_limit = 0;
  if (!dalvik_limit)
      dalvik_limit = (base::SysInfo::DalvikHeapSizeMB() / 2) * 1024 * 1024;
  return std::min(viewport_limit, dalvik_limit);
#endif
}

size_t GpuMemoryManager::CalcAvailableFromGpuTotal(size_t total_gpu_memory) {
  // Allow Chrome to use 75% of total GPU memory, or all-but-64MB of GPU
  // memory, whichever is less.
  return std::min(3 * total_gpu_memory / 4, total_gpu_memory - 64*1024*1024);
}

void GpuMemoryManager::UpdateAvailableGpuMemory() {
  // If the amount of video memory to use was specified at the command
  // line, never change it.
  if (bytes_available_gpu_memory_overridden_)
    return;

#if defined(OS_ANDROID)
  // On Android we use the surface size, so this finds the largest visible
  // surface size instead of lowest gpu's limit.
  int max_surface_area = 0;
#else
  // On non-Android, we use an operating system query when possible.
  // We do not have a reliable concept of multiple GPUs existing in
  // a system, so just be safe and go with the minimum encountered.
  size_t bytes_min = 0;
#endif

  // Only use the clients that are visible, because otherwise the set of clients
  // we are querying could become extremely large.
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
      it != clients_visible_mru_.end();
      ++it) {
    const GpuMemoryManagerClientState* client_state = *it;
    if (!client_state->has_surface_)
      continue;
    if (!client_state->visible_)
      continue;

#if defined(OS_ANDROID)
    gfx::Size surface_size = client_state->client_->GetSurfaceSize();
    max_surface_area = std::max(max_surface_area, surface_size.width() *
                                                  surface_size.height());
#else
    size_t bytes = 0;
    if (client_state->client_->GetTotalGpuMemory(&bytes)) {
      if (!bytes_min || bytes < bytes_min)
        bytes_min = bytes;
    }
#endif
  }

#if defined(OS_ANDROID)
  bytes_available_gpu_memory_ = CalcAvailableFromViewportArea(max_surface_area);
#else
  if (!bytes_min)
    return;

  bytes_available_gpu_memory_ = CalcAvailableFromGpuTotal(bytes_min);
#endif

  // Never go below the default allocation
  bytes_available_gpu_memory_ = std::max(bytes_available_gpu_memory_,
                                         GetDefaultAvailableGpuMemory());

  // Never go above the maximum.
  bytes_available_gpu_memory_ = std::min(bytes_available_gpu_memory_,
                                         GetMaximumTotalGpuMemory());
}

void GpuMemoryManager::UpdateUnmanagedMemoryLimits() {
  // Set the limit to be [current_, current_ + step_ / 4), with the endpoints
  // of the intervals rounded down and up to the nearest step_, to avoid
  // thrashing the interval.
  bytes_allocated_unmanaged_high_ = RoundUp(
      bytes_allocated_unmanaged_current_ + bytes_unmanaged_limit_step_ / 4,
      bytes_unmanaged_limit_step_);
  bytes_allocated_unmanaged_low_ = RoundDown(
      bytes_allocated_unmanaged_current_,
      bytes_unmanaged_limit_step_);
}

void GpuMemoryManager::UpdateBackgroundedAvailableGpuMemory() {
  // Be conservative and disable saving backgrounded tabs' textures on Android
  // for the moment
#if defined(OS_ANDROID)
  bytes_backgrounded_available_gpu_memory_ = 0;
#else
  bytes_backgrounded_available_gpu_memory_ = GetAvailableGpuMemory() / 4;
#endif
}

void GpuMemoryManager::ScheduleManage(bool immediate) {
  if (disable_schedule_manage_)
    return;
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

void GpuMemoryManager::TrackMemoryAllocatedChange(
    GpuMemoryTrackingGroup* tracking_group,
    size_t old_size,
    size_t new_size,
    gpu::gles2::MemoryTracker::Pool tracking_pool) {
  TrackValueChanged(old_size, new_size, &tracking_group->size_);
  switch (tracking_pool) {
    case gpu::gles2::MemoryTracker::kManaged:
      TrackValueChanged(old_size, new_size, &bytes_allocated_managed_current_);
      break;
    case gpu::gles2::MemoryTracker::kUnmanaged:
      TrackValueChanged(old_size,
                        new_size,
                        &bytes_allocated_unmanaged_current_);
      break;
    default:
      NOTREACHED();
      break;
  }
  if (new_size != old_size) {
    TRACE_COUNTER1("gpu",
                   "GpuMemoryUsage",
                   GetCurrentUsage());
  }

  // If we've gone past our current limit on unmanaged memory, schedule a
  // re-manage to take int account the unmanaged memory.
  if (bytes_allocated_unmanaged_current_ >= bytes_allocated_unmanaged_high_)
    ScheduleManage(true);
  if (bytes_allocated_unmanaged_current_ < bytes_allocated_unmanaged_low_)
    ScheduleManage(false);

  if (GetCurrentUsage() > bytes_allocated_historical_max_) {
      bytes_allocated_historical_max_ = GetCurrentUsage();
      // If we're blowing into new memory usage territory, spam the browser
      // process with the most up-to-date information about our memory usage.
      SendUmaStatsToBrowser();
  }
}

GpuMemoryManagerClientState* GpuMemoryManager::CreateClientState(
    GpuMemoryManagerClient* client,
    bool has_surface,
    bool visible) {
  TrackingGroupMap::iterator tracking_group_it =
      tracking_groups_.find(client->GetMemoryTracker());
  DCHECK(tracking_group_it != tracking_groups_.end());
  GpuMemoryTrackingGroup* tracking_group = tracking_group_it->second;

  GpuMemoryManagerClientState* client_state = new GpuMemoryManagerClientState(
      this, client, tracking_group, has_surface, visible);
  TrackValueChanged(0, client_state->managed_memory_stats_.bytes_allocated,
                    client_state->visible_ ?
                        &bytes_allocated_managed_visible_ :
                        &bytes_allocated_managed_backgrounded_);
  AddClientToList(client_state);
  ScheduleManage(true);
  return client_state;
}

void GpuMemoryManager::OnDestroyClientState(
    GpuMemoryManagerClientState* client_state) {
  RemoveClientFromList(client_state);
  TrackValueChanged(client_state->managed_memory_stats_.bytes_allocated, 0,
                    client_state->visible_ ?
                        &bytes_allocated_managed_visible_ :
                        &bytes_allocated_managed_backgrounded_);
  ScheduleManage(false);
}

void GpuMemoryManager::SetClientStateVisible(
    GpuMemoryManagerClientState* client_state, bool visible) {
  DCHECK(client_state->has_surface_);
  if (client_state->visible_ == visible)
    return;

  RemoveClientFromList(client_state);
  client_state->visible_ = visible;
  AddClientToList(client_state);

  TrackValueChanged(client_state->managed_memory_stats_.bytes_allocated, 0,
                    client_state->visible_ ?
                        &bytes_allocated_managed_backgrounded_ :
                        &bytes_allocated_managed_visible_);
  TrackValueChanged(0, client_state->managed_memory_stats_.bytes_allocated,
                    client_state->visible_ ?
                        &bytes_allocated_managed_visible_ :
                        &bytes_allocated_managed_backgrounded_);
  ScheduleManage(visible);
}

void GpuMemoryManager::SetClientStateManagedMemoryStats(
    GpuMemoryManagerClientState* client_state,
    const GpuManagedMemoryStats& stats)
{
  TrackValueChanged(client_state->managed_memory_stats_.bytes_allocated,
                    stats.bytes_allocated,
                    client_state->visible_ ?
                        &bytes_allocated_managed_visible_ :
                        &bytes_allocated_managed_backgrounded_);
  client_state->managed_memory_stats_ = stats;

  // If this allocation pushed our usage of backgrounded tabs memory over the
  // limit, then schedule a drop of backgrounded memory.
  if (bytes_allocated_managed_backgrounded_ >
      GetCurrentBackgroundedAvailableGpuMemory())
    ScheduleManage(false);
}

GpuMemoryTrackingGroup* GpuMemoryManager::CreateTrackingGroup(
    base::ProcessId pid, gpu::gles2::MemoryTracker* memory_tracker) {
  GpuMemoryTrackingGroup* tracking_group = new GpuMemoryTrackingGroup(
      pid, memory_tracker, this);
  DCHECK(!tracking_groups_.count(tracking_group->GetMemoryTracker()));
  tracking_groups_.insert(std::make_pair(tracking_group->GetMemoryTracker(),
                                         tracking_group));
  return tracking_group;
}

void GpuMemoryManager::OnDestroyTrackingGroup(
    GpuMemoryTrackingGroup* tracking_group) {
  DCHECK(tracking_groups_.count(tracking_group->GetMemoryTracker()));
  tracking_groups_.erase(tracking_group->GetMemoryTracker());
}

void GpuMemoryManager::GetVideoMemoryUsageStats(
    GPUVideoMemoryUsageStats* video_memory_usage_stats) const {
  // For each context group, assign its memory usage to its PID
  video_memory_usage_stats->process_map.clear();
  for (TrackingGroupMap::const_iterator i =
       tracking_groups_.begin(); i != tracking_groups_.end(); ++i) {
    const GpuMemoryTrackingGroup* tracking_group = i->second;
    video_memory_usage_stats->process_map[
        tracking_group->GetPid()].video_memory += tracking_group->GetSize();
  }

  // Assign the total across all processes in the GPU process
  video_memory_usage_stats->process_map[
      base::GetCurrentProcId()].video_memory = GetCurrentUsage();
  video_memory_usage_stats->process_map[
      base::GetCurrentProcId()].has_duplicates = true;
}

void GpuMemoryManager::SetWindowCount(uint32 window_count) {
  bool should_schedule_manage = !window_count_has_been_received_ ||
                                (window_count != window_count_);
  window_count_has_been_received_ = true;
  window_count_ = window_count;
  if (should_schedule_manage)
    ScheduleManage(true);
}

// The current Manage algorithm simply classifies contexts (clients) into
// "foreground", "background", or "hibernated" categories.
// For each of these three categories, there are predefined memory allocation
// limits and front/backbuffer states.
//
// Users may or may not have a surfaces, and the rules are different for each.
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

  // Update the amount of GPU memory available on the system.
  UpdateAvailableGpuMemory();

  // Update the limit on unmanaged memory.
  UpdateUnmanagedMemoryLimits();

  // Update the backgrounded available gpu memory because it depends on
  // the available GPU memory.
  UpdateBackgroundedAvailableGpuMemory();

  // Determine which clients are "hibernated" (which determines the
  // distribution of frontbuffers and memory among clients that don't have
  // surfaces).
  SetClientsHibernatedState();

  // Determine how much memory to assign to give to visible and backgrounded
  // clients.
  size_t bytes_limit_when_visible = GetVisibleClientAllocation();

  // Experiment to determine if aggressively discarding tiles on OS X
  // results in greater stability.
#if defined(OS_MACOSX)
  GpuMemoryAllocationForRenderer::PriorityCutoff priority_cutoff_when_visible =
      GpuMemoryAllocationForRenderer::kPriorityCutoffAllowNiceToHave;
#else
  GpuMemoryAllocationForRenderer::PriorityCutoff priority_cutoff_when_visible =
      GpuMemoryAllocationForRenderer::kPriorityCutoffAllowEverything;
#endif

  // Assign memory allocations to visible clients.
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
       it != clients_visible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    GpuMemoryAllocation allocation;

    allocation.browser_allocation.suggest_have_frontbuffer = true;
    allocation.renderer_allocation.bytes_limit_when_visible =
        bytes_limit_when_visible;
    allocation.renderer_allocation.priority_cutoff_when_visible =
        priority_cutoff_when_visible;

    // Allow this client to keep its textures when backgrounded if they
    // aren't so expensive that they won't fit.
    if (client_state->managed_memory_stats_.bytes_required <=
        bytes_backgrounded_available_gpu_memory_) {
      allocation.renderer_allocation.bytes_limit_when_not_visible =
          GetCurrentBackgroundedAvailableGpuMemory();
      allocation.renderer_allocation.priority_cutoff_when_not_visible =
          GpuMemoryAllocationForRenderer::kPriorityCutoffAllowOnlyRequired;
    } else {
        allocation.renderer_allocation.bytes_limit_when_not_visible = 0;
        allocation.renderer_allocation.priority_cutoff_when_not_visible =
            GpuMemoryAllocationForRenderer::kPriorityCutoffAllowNothing;
    }

    client_state->client_->SetMemoryAllocation(allocation);
  }

  // Assign memory allocations to backgrounded clients.
  size_t bytes_allocated_backgrounded = 0;
  for (ClientStateList::const_iterator it = clients_nonvisible_mru_.begin();
       it != clients_nonvisible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    GpuMemoryAllocation allocation;

    allocation.browser_allocation.suggest_have_frontbuffer =
        !client_state->hibernated_;
    allocation.renderer_allocation.bytes_limit_when_visible =
        bytes_limit_when_visible;
    allocation.renderer_allocation.priority_cutoff_when_visible =
        priority_cutoff_when_visible;

    if (client_state->managed_memory_stats_.bytes_required +
        bytes_allocated_backgrounded <=
        GetCurrentBackgroundedAvailableGpuMemory()) {
      bytes_allocated_backgrounded +=
          client_state->managed_memory_stats_.bytes_required;
      allocation.renderer_allocation.bytes_limit_when_not_visible =
          GetCurrentBackgroundedAvailableGpuMemory();
      allocation.renderer_allocation.priority_cutoff_when_not_visible =
          GpuMemoryAllocationForRenderer::kPriorityCutoffAllowOnlyRequired;
    } else {
      allocation.renderer_allocation.bytes_limit_when_not_visible = 0;
      allocation.renderer_allocation.priority_cutoff_when_not_visible =
          GpuMemoryAllocationForRenderer::kPriorityCutoffAllowNothing;
    }

    client_state->client_->SetMemoryAllocation(allocation);
  }

  // Assign memory allocations to clients that don't have surfaces.
  for (ClientStateList::const_iterator it = clients_nonsurface_.begin();
       it != clients_nonsurface_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    GpuMemoryAllocation allocation;

    if (!client_state->hibernated_) {
      allocation.renderer_allocation.bytes_limit_when_visible =
          GetMinimumTabAllocation();
      allocation.renderer_allocation.priority_cutoff_when_visible =
          GpuMemoryAllocationForRenderer::kPriorityCutoffAllowEverything;
    }

    client_state->client_->SetMemoryAllocation(allocation);
  }

  SendUmaStatsToBrowser();
}

void GpuMemoryManager::SetClientsHibernatedState() const {
  // Re-set all tracking groups as being hibernated.
  for (TrackingGroupMap::const_iterator it = tracking_groups_.begin();
       it != tracking_groups_.end();
       ++it) {
    GpuMemoryTrackingGroup* tracking_group = it->second;
    tracking_group->hibernated_ = true;
  }
  // All clients with surfaces that are visible are non-hibernated.
  size_t non_hibernated_clients = 0;
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
       it != clients_visible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    client_state->hibernated_ = false;
    client_state->tracking_group_->hibernated_ = false;
    non_hibernated_clients++;
  }
  // Then an additional few clients with surfaces are non-hibernated too, up to
  // a fixed limit.
  for (ClientStateList::const_iterator it = clients_nonvisible_mru_.begin();
       it != clients_nonvisible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    if (non_hibernated_clients < max_surfaces_with_frontbuffer_soft_limit_) {
      client_state->hibernated_ = false;
      client_state->tracking_group_->hibernated_ = false;
      non_hibernated_clients++;
    } else {
      client_state->hibernated_ = true;
    }
  }
  // Clients that don't have surfaces are non-hibernated if they are
  // in a GL share group with a non-hibernated surface.
  for (ClientStateList::const_iterator it = clients_nonsurface_.begin();
       it != clients_nonsurface_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    client_state->hibernated_ = client_state->tracking_group_->hibernated_;
  }
}

size_t GpuMemoryManager::GetVisibleClientAllocation() const {
  // Count how many clients will get allocations.
  size_t clients_with_surface_visible_count = clients_visible_mru_.size();
  size_t clients_without_surface_not_hibernated_count = 0;
  for (ClientStateList::const_iterator it = clients_nonsurface_.begin();
       it != clients_nonsurface_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    if (!client_state->hibernated_)
      clients_without_surface_not_hibernated_count++;
  }

  // Calculate bonus allocation by splitting remainder of global limit equally
  // after giving out the minimum to those that need it.
  size_t num_clients_need_mem = clients_with_surface_visible_count +
                                clients_without_surface_not_hibernated_count;
  size_t base_allocation_size = GetMinimumTabAllocation() *
                                num_clients_need_mem;
  size_t bonus_allocation = 0;
  if (base_allocation_size < GetAvailableGpuMemory() &&
      clients_with_surface_visible_count)
    bonus_allocation = (GetAvailableGpuMemory() - base_allocation_size) /
                       clients_with_surface_visible_count;
  size_t clients_allocation_when_visible = GetMinimumTabAllocation() +
                                           bonus_allocation;

  // If we have received a window count message, then override the client-based
  // scheme with a per-window scheme
  if (window_count_has_been_received_) {
    clients_allocation_when_visible = std::max(
        clients_allocation_when_visible,
        GetAvailableGpuMemory() / std::max(window_count_, 1u));
  }

  // Limit the memory per client to its maximum allowed level.
  if (clients_allocation_when_visible >= GetMaximumTabAllocation())
    clients_allocation_when_visible = GetMaximumTabAllocation();

  return clients_allocation_when_visible;
}

void GpuMemoryManager::SendUmaStatsToBrowser() {
  if (!channel_manager_)
    return;
  GPUMemoryUmaStats params;
  params.bytes_allocated_current = GetCurrentUsage();
  params.bytes_allocated_max = bytes_allocated_historical_max_;
  params.bytes_limit = bytes_available_gpu_memory_;
  params.window_count = window_count_;
  channel_manager_->Send(new GpuHostMsg_GpuMemoryUmaStats(params));
}

GpuMemoryManager::ClientStateList* GpuMemoryManager::GetClientList(
    GpuMemoryManagerClientState* client_state) {
  if (client_state->has_surface_) {
    if (client_state->visible_)
      return &clients_visible_mru_;
    else
      return &clients_nonvisible_mru_;
  }
  return &clients_nonsurface_;
}

void GpuMemoryManager::AddClientToList(
    GpuMemoryManagerClientState* client_state) {
  DCHECK(!client_state->list_iterator_valid_);
  ClientStateList* client_list = GetClientList(client_state);
  client_state->list_iterator_ = client_list->insert(
      client_list->begin(), client_state);
  client_state->list_iterator_valid_ = true;
}

void GpuMemoryManager::RemoveClientFromList(
    GpuMemoryManagerClientState* client_state) {
  DCHECK(client_state->list_iterator_valid_);
  ClientStateList* client_list = GetClientList(client_state);
  client_list->erase(client_state->list_iterator_);
  client_state->list_iterator_valid_ = false;
}

}  // namespace content

#endif
