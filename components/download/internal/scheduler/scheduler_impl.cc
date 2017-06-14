// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/scheduler/scheduler_impl.h"

#include "components/download/internal/client_set.h"
#include "components/download/internal/entry_utils.h"
#include "components/download/internal/scheduler/device_status.h"
#include "components/download/public/download_params.h"

namespace download {

namespace {

// Returns a vector of elements contained in the |set|.
template <typename T>
std::vector<T> ToList(const std::set<T>& set) {
  std::vector<T> list;
  for (const auto& element : set) {
    list.push_back(element);
  }
  return list;
}

}  // namespace

SchedulerImpl::SchedulerImpl(PlatformTaskScheduler* platform_scheduler,
                             const ClientSet* clients)
    : SchedulerImpl(platform_scheduler,
                    ToList<DownloadClient>(clients->GetRegisteredClients())) {}

SchedulerImpl::SchedulerImpl(PlatformTaskScheduler* platform_scheduler,
                             const std::vector<DownloadClient>& clients)
    : platform_scheduler_(platform_scheduler),
      download_clients_(clients),
      current_client_index_(0) {
  DCHECK(!clients.empty());
}

SchedulerImpl::~SchedulerImpl() = default;

void SchedulerImpl::Reschedule(const Model::EntryList& entries) {
  if (entries.empty()) {
    if (platform_scheduler_)
      platform_scheduler_->CancelDownloadTask();
    return;
  }

  // TODO(xingliu): Figure out if we need to pass the time window to platform
  // scheduler, and support NetworkRequirements::OPTIMISTIC.
  if (platform_scheduler_) {
    platform_scheduler_->CancelDownloadTask();
    platform_scheduler_->ScheduleDownloadTask(
        util::GetSchedulingCriteria(entries));
  }
}

Entry* SchedulerImpl::Next(const Model::EntryList& entries,
                           const DeviceStatus& device_status) {
  std::map<DownloadClient, Entry*> candidates =
      FindCandidates(entries, device_status);

  Entry* entry = nullptr;
  size_t index = current_client_index_;

  // Finds the next entry to download.
  for (size_t i = 0; i < download_clients_.size(); ++i) {
    DownloadClient client =
        download_clients_[(index + i) % download_clients_.size()];
    Entry* candidate = candidates[client];

    // Some clients may have no entries, continue to check other clients.
    if (!candidate)
      continue;

    bool ui_priority =
        candidate->scheduling_params.priority == SchedulingParams::Priority::UI;

    // Records the first available candidate. Keep iterating to see if there
    // are UI priority entries for other clients.
    if (!entry || ui_priority) {
      entry = candidate;

      // Load balancing between clients.
      current_client_index_ = (index + i + 1) % download_clients_.size();

      // UI priority entry will be processed immediately.
      if (ui_priority)
        break;
    }
  }
  return entry;
}

std::map<DownloadClient, Entry*> SchedulerImpl::FindCandidates(
    const Model::EntryList& entries,
    const DeviceStatus& device_status) {
  std::map<DownloadClient, Entry*> candidates;

  if (entries.empty())
    return candidates;

  for (auto* const entry : entries) {
    DCHECK(entry);
    const SchedulingParams& current_params = entry->scheduling_params;

    // Every download needs to pass the state and device status check.
    if (entry->state != Entry::State::AVAILABLE ||
        !device_status.MeetsCondition(current_params).MeetsRequirements()) {
      continue;
    }

    // Find the most appropriate download based on priority and cancel time.
    Entry* candidate = candidates[entry->client];
    if (!candidate || util::EntryBetterThan(*entry, *candidate)) {
      candidates[entry->client] = entry;
    }
  }

  return candidates;
}

}  // namespace download
