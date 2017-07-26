// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_background_task_handler_impl.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_background_task.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/backoff_entry_serializer.h"

namespace offline_pages {

const net::BackoffEntry::Policy kBackoffPolicy = {
    0,                 // Number of initial errors to ignore without backoff.
    30 * 1000,         // Initial delay for backoff in ms: 30 seconds.
    2,                 // Factor to multiply for exponential backoff.
    0.33,              // Fuzzing percentage.
    24 * 3600 * 1000,  // Maximum time to delay requests in ms: 1 day.
    -1,                // Don't discard entry even if unused.
    false              // Don't use initial delay unless the last was an error.
};

PrefetchBackgroundTaskHandlerImpl::PrefetchBackgroundTaskHandlerImpl(
    PrefService* prefs)
    : prefs_(prefs) {}
PrefetchBackgroundTaskHandlerImpl::~PrefetchBackgroundTaskHandlerImpl() =
    default;

void PrefetchBackgroundTaskHandlerImpl::CancelBackgroundTask() {
  PrefetchBackgroundTask::Cancel();
}

int PrefetchBackgroundTaskHandlerImpl::GetAdditionalBackoffSeconds() const {
  return static_cast<int>(
      GetCurrentBackoff()->GetTimeUntilRelease().InSeconds());
}

void PrefetchBackgroundTaskHandlerImpl::EnsureTaskScheduled() {
  int seconds_until_release = 0;
  std::unique_ptr<net::BackoffEntry> backoff = GetCurrentBackoff();
  if (backoff)
    seconds_until_release = backoff->GetTimeUntilRelease().InSeconds();
  PrefetchBackgroundTask::Schedule(seconds_until_release,
                                   false /*update_current*/);
}

std::unique_ptr<net::BackoffEntry>
PrefetchBackgroundTaskHandlerImpl::GetCurrentBackoff() const {
  const base::ListValue* value =
      prefs_->GetList(prefs::kOfflinePrefetchBackoff);
  std::unique_ptr<net::BackoffEntry> result;
  if (value) {
    result = net::BackoffEntrySerializer::DeserializeFromValue(
        *value, &kBackoffPolicy, clock_, base::Time::Now());
  }
  if (!result)
    return base::MakeUnique<net::BackoffEntry>(&kBackoffPolicy, clock_);
  return result;
}

void PrefetchBackgroundTaskHandlerImpl::Backoff() {
  std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
  current->InformOfRequest(false);
  UpdateBackoff(current.get());
}

void PrefetchBackgroundTaskHandlerImpl::ResetBackoff() {
  std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
  current->Reset();
  UpdateBackoff(current.get());
}

void PrefetchBackgroundTaskHandlerImpl::SetTickClockForTesting(
    base::TickClock* clock) {
  clock_ = clock;
}

void PrefetchBackgroundTaskHandlerImpl::UpdateBackoff(
    net::BackoffEntry* backoff) {
  std::unique_ptr<base::Value> value =
      net::BackoffEntrySerializer::SerializeToValue(*backoff,
                                                    base::Time::Now());
  prefs_->Set(prefs::kOfflinePrefetchBackoff, *value);
}

void RegisterPrefetchBackgroundTaskPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kOfflinePrefetchBackoff);
}

}  // namespace offline_pages
