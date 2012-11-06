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
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "gpu/command_buffer/service/gpu_switches.h"

namespace content {
namespace {

const int kDelayedScheduleManageTimeoutMs = 67;

}

void GpuMemoryManager::AssignMemoryAllocations(
    const ClientStateVector& clients,
    const GpuMemoryAllocation& allocation) {
  for (GpuMemoryManager::ClientStateVector::const_iterator it = clients.begin();
      it != clients.end();
      ++it) {
    (*it)->client->SetMemoryAllocation(allocation);
  }
}

GpuMemoryManager::GpuMemoryManager(
    size_t max_surfaces_with_frontbuffer_soft_limit)
    : manage_immediate_scheduled_(false),
      max_surfaces_with_frontbuffer_soft_limit_(
          max_surfaces_with_frontbuffer_soft_limit),
      bytes_available_gpu_memory_(0),
      bytes_available_gpu_memory_overridden_(false),
      bytes_allocated_current_(0),
      bytes_allocated_historical_max_(0),
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
}

GpuMemoryManager::~GpuMemoryManager() {
  DCHECK(tracking_groups_.empty());
  DCHECK(clients_.empty());
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

void GpuMemoryManager::UpdateAvailableGpuMemory(
   const ClientStateVector& clients) {
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
  for (ClientStateVector::const_iterator it = clients.begin();
      it != clients.end(); ++it) {
    ClientState* client_state = *it;
    if (!client_state->has_surface)
      continue;
    if (!client_state->visible)
      continue;

#if defined(OS_ANDROID)
    gfx::Size surface_size = client_state->client->GetSurfaceSize();
    max_surface_area = std::max(max_surface_area, surface_size.width() *
                                                  surface_size.height());
#else
    size_t bytes = 0;
    if (client_state->client->GetTotalGpuMemory(&bytes)) {
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

bool GpuMemoryManager::ClientsWithSurfaceComparator::operator()(
    ClientState* lhs,
    ClientState* rhs) {
  DCHECK(lhs->has_surface && rhs->has_surface);
  if (lhs->visible)
    return !rhs->visible || (lhs->last_used_time > rhs->last_used_time);
  else
    return !rhs->visible && (lhs->last_used_time > rhs->last_used_time);
};

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
    TRACE_COUNTER1("gpu",
                   "GpuMemoryUsage",
                   bytes_allocated_current_);
  }
}

void GpuMemoryManager::AddClient(GpuMemoryManagerClient* client,
                                 bool has_surface,
                                 bool visible,
                                 base::TimeTicks last_used_time) {
  if (clients_.count(client))
    return;
  std::pair<GpuMemoryManagerClient*, ClientState*> entry(
     client,
     new ClientState(client, has_surface, visible, last_used_time));
  clients_.insert(entry);

  ScheduleManage(true);
}

void GpuMemoryManager::RemoveClient(GpuMemoryManagerClient* client) {
  ClientMap::iterator it = clients_.find(client);
  if (it == clients_.end())
    return;
  delete it->second;
  clients_.erase(it);

  ScheduleManage(false);
}

void GpuMemoryManager::SetClientVisible(GpuMemoryManagerClient* client,
                                        bool visible)
{
  ClientMap::const_iterator it = clients_.find(client);
  if (it == clients_.end())
    return;
  ClientState* client_state = it->second;

  client_state->visible = visible;
  ScheduleManage(visible);
}

void GpuMemoryManager::SetClientManagedMemoryStats(
    GpuMemoryManagerClient* client,
    const GpuManagedMemoryStats& stats)
{
  ClientMap::const_iterator it = clients_.find(client);
  if (it == clients_.end())
    return;
  ClientState* client_state = it->second;

  client_state->managed_memory_stats = stats;
}

void GpuMemoryManager::TestingSetClientVisible(
    GpuMemoryManagerClient* client, bool visible) {
  DCHECK(clients_.count(client));
  clients_[client]->visible = visible;
}

void GpuMemoryManager::TestingSetClientLastUsedTime(
    GpuMemoryManagerClient* client, base::TimeTicks last_used_time) {
  DCHECK(clients_.count(client));
  clients_[client]->last_used_time = last_used_time;
}

void GpuMemoryManager::TestingSetClientHasSurface(
    GpuMemoryManagerClient* client, bool has_surface) {
  DCHECK(clients_.count(client));
  clients_[client]->has_surface = has_surface;
}

bool GpuMemoryManager::TestingCompareClients(
    GpuMemoryManagerClient* lhs, GpuMemoryManagerClient* rhs) const {
  ClientMap::const_iterator it_lhs = clients_.find(lhs);
  ClientMap::const_iterator it_rhs = clients_.find(rhs);
  DCHECK(it_lhs != clients_.end());
  DCHECK(it_rhs != clients_.end());
  ClientsWithSurfaceComparator comparator;
  return comparator.operator()(it_lhs->second, it_rhs->second);
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
    GPUVideoMemoryUsageStats& video_memory_usage_stats) const {
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

  // Create client lists by separating out the two types received from client
  ClientStateVector clients_with_surface;
  ClientStateVector clients_without_surface;
  {
    for (ClientMap::iterator it = clients_.begin();
         it != clients_.end(); ++it) {
      ClientState* client_state = it->second;
      if (client_state->has_surface)
        clients_with_surface.push_back(client_state);
      else
        clients_without_surface.push_back(client_state);
    }
  }

  // Sort clients with surface into {visibility,last_used_time} order using
  // custom comparator
  std::sort(clients_with_surface.begin(),
            clients_with_surface.end(),
            ClientsWithSurfaceComparator());
  DCHECK(
      std::unique(clients_with_surface.begin(), clients_with_surface.end()) ==
          clients_with_surface.end());

  std::set<gpu::gles2::MemoryTracker*> memory_trackers_not_hibernated;

  // Separate the clients with surfaces into the ones that will get a
  // frontbuffer (the ones that are visible, and then the most recently
  // used, up to a limit) and those that do not get a frontbuffer.
  size_t clients_with_surface_visible_count = 0;
  ClientStateVector clients_with_surface_not_hibernated;
  ClientStateVector clients_with_surface_hibernated;
  for (size_t i = 0; i < clients_with_surface.size(); ++i) {
    ClientState* client_state = clients_with_surface[i];
    DCHECK(client_state->has_surface);

    if (client_state->visible)
      clients_with_surface_visible_count++;

    if (client_state->visible ||
        i < max_surfaces_with_frontbuffer_soft_limit_) {
      memory_trackers_not_hibernated.insert(
          client_state->client->GetMemoryTracker());
      clients_with_surface_not_hibernated.push_back(client_state);
    } else {
      clients_with_surface_hibernated.push_back(client_state);
    }
  }

  // Separate the clients without surfaces into the ones that will be given a
  // memory allocation (the ones that are in a share group with a client with
  // a surface that has a frontbuffer) and those that won't.
  ClientStateVector clients_without_surface_not_hibernated;
  ClientStateVector clients_without_surface_hibernated;
  for (ClientStateVector::const_iterator it = clients_without_surface.begin();
      it != clients_without_surface.end(); ++it) {
    ClientState* client_state = *it;
    DCHECK(!client_state->has_surface);
    if (memory_trackers_not_hibernated.count(
        client_state->client->GetMemoryTracker()))
      clients_without_surface_not_hibernated.push_back(client_state);
    else
      clients_without_surface_hibernated.push_back(client_state);
  }

  // Update the amount of GPU memory available on the system.
  UpdateAvailableGpuMemory(clients_with_surface);

  // Calculate bonus allocation by splitting remainder of global limit equally
  // after giving out the minimum to those that need it.
  size_t num_clients_need_mem = clients_with_surface_visible_count +
                                clients_without_surface_not_hibernated.size();
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
       GetAvailableGpuMemory()/std::max(window_count_, 1u));
  }

  // Limit the memory per client to its maximum allowed level.
  if (clients_allocation_when_visible >= GetMaximumTabAllocation())
    clients_allocation_when_visible = GetMaximumTabAllocation();

  // Now give out allocations to everyone.
  AssignMemoryAllocations(
      clients_with_surface_not_hibernated,
      GpuMemoryAllocation(clients_allocation_when_visible,
                          GpuMemoryAllocation::kHasFrontbuffer));

  AssignMemoryAllocations(
      clients_with_surface_hibernated,
      GpuMemoryAllocation(clients_allocation_when_visible,
                          GpuMemoryAllocation::kHasNoFrontbuffer));

  AssignMemoryAllocations(
      clients_without_surface_not_hibernated,
      GpuMemoryAllocation(GetMinimumTabAllocation(),
                          GpuMemoryAllocation::kHasNoFrontbuffer));

  AssignMemoryAllocations(
      clients_without_surface_hibernated,
      GpuMemoryAllocation(0, GpuMemoryAllocation::kHasNoFrontbuffer));
}

GpuMemoryManager::ClientState::ClientState(
    GpuMemoryManagerClient* client,
    bool has_surface,
    bool visible,
    base::TimeTicks last_used_time)
    : client(client),
      has_surface(has_surface),
      visible(visible),
      last_used_time(last_used_time) {
}

}  // namespace content

#endif
