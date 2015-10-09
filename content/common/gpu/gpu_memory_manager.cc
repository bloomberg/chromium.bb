// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_memory_manager_client.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "content/common/gpu/gpu_memory_uma_stats.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/gpu_switches.h"

using gpu::MemoryAllocation;

namespace content {
namespace {

const int kDelayedScheduleManageTimeoutMs = 67;

const uint64 kBytesAllocatedStep = 16 * 1024 * 1024;

void TrackValueChanged(uint64 old_size, uint64 new_size, uint64* total_size) {
  DCHECK(new_size > old_size || *total_size >= (old_size - new_size));
  *total_size += (new_size - old_size);
}

}

GpuMemoryManager::GpuMemoryManager(
    GpuChannelManager* channel_manager,
    uint64 max_surfaces_with_frontbuffer_soft_limit)
    : channel_manager_(channel_manager),
      manage_immediate_scheduled_(false),
      disable_schedule_manage_(false),
      max_surfaces_with_frontbuffer_soft_limit_(
          max_surfaces_with_frontbuffer_soft_limit),
      client_hard_limit_bytes_(0),
      bytes_allocated_current_(0),
      bytes_allocated_historical_max_(0)
{ }

GpuMemoryManager::~GpuMemoryManager() {
  DCHECK(tracking_groups_.empty());
  DCHECK(clients_visible_mru_.empty());
  DCHECK(clients_nonvisible_mru_.empty());
  DCHECK(clients_nonsurface_.empty());
  DCHECK(!bytes_allocated_current_);
}

void GpuMemoryManager::ScheduleManage(
    ScheduleManageTime schedule_manage_time) {
  if (disable_schedule_manage_)
    return;
  if (manage_immediate_scheduled_)
    return;
  if (schedule_manage_time == kScheduleManageNow) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&GpuMemoryManager::Manage, AsWeakPtr()));
    manage_immediate_scheduled_ = true;
    if (!delayed_manage_callback_.IsCancelled())
      delayed_manage_callback_.Cancel();
  } else {
    if (!delayed_manage_callback_.IsCancelled())
      return;
    delayed_manage_callback_.Reset(base::Bind(&GpuMemoryManager::Manage,
                                              AsWeakPtr()));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, delayed_manage_callback_.callback(),
        base::TimeDelta::FromMilliseconds(kDelayedScheduleManageTimeoutMs));
  }
}

void GpuMemoryManager::TrackMemoryAllocatedChange(
    GpuMemoryTrackingGroup* tracking_group,
    uint64 old_size,
    uint64 new_size) {
  TrackValueChanged(old_size, new_size, &tracking_group->size_);
  TrackValueChanged(old_size, new_size, &bytes_allocated_current_);

  if (GetCurrentUsage() > bytes_allocated_historical_max_ +
                          kBytesAllocatedStep) {
      bytes_allocated_historical_max_ = GetCurrentUsage();
      // If we're blowing into new memory usage territory, spam the browser
      // process with the most up-to-date information about our memory usage.
      SendUmaStatsToBrowser();
  }
}

bool GpuMemoryManager::EnsureGPUMemoryAvailable(uint64 /* size_needed */) {
  // TODO: Check if there is enough space. Lose contexts until there is.
  return true;
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
  AddClientToList(client_state);
  ScheduleManage(kScheduleManageNow);
  return client_state;
}

void GpuMemoryManager::OnDestroyClientState(
    GpuMemoryManagerClientState* client_state) {
  RemoveClientFromList(client_state);
  ScheduleManage(kScheduleManageLater);
}

void GpuMemoryManager::SetClientStateVisible(
    GpuMemoryManagerClientState* client_state, bool visible) {
  DCHECK(client_state->has_surface_);
  if (client_state->visible_ == visible)
    return;

  RemoveClientFromList(client_state);
  client_state->visible_ = visible;
  AddClientToList(client_state);
  ScheduleManage(visible ? kScheduleManageNow : kScheduleManageLater);
}

uint64 GpuMemoryManager::GetTrackerMemoryUsage(
    gpu::gles2::MemoryTracker* tracker) const {
  TrackingGroupMap::const_iterator tracking_group_it =
      tracking_groups_.find(tracker);
  DCHECK(tracking_group_it != tracking_groups_.end());
  return tracking_group_it->second->GetSize();
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

  video_memory_usage_stats->bytes_allocated = GetCurrentUsage();
  video_memory_usage_stats->bytes_allocated_historical_max =
      bytes_allocated_historical_max_;
}

void GpuMemoryManager::Manage() {
  manage_immediate_scheduled_ = false;
  delayed_manage_callback_.Cancel();

  // Determine which clients are "hibernated" (which determines the
  // distribution of frontbuffers and memory among clients that don't have
  // surfaces).
  SetClientsHibernatedState();

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
  uint64 non_hibernated_clients = 0;
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

void GpuMemoryManager::SendUmaStatsToBrowser() {
  if (!channel_manager_)
    return;
  GPUMemoryUmaStats params;
  params.bytes_allocated_current = GetCurrentUsage();
  params.bytes_allocated_max = bytes_allocated_historical_max_;
  params.bytes_limit = client_hard_limit_bytes_;
  params.client_count = clients_visible_mru_.size() +
                        clients_nonvisible_mru_.size() +
                        clients_nonsurface_.size();
  params.context_group_count = tracking_groups_.size();
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
