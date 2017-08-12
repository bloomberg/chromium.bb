// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/session_duration_updater.h"

#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace feature_engagement {

SessionDurationUpdater::SessionDurationUpdater(PrefService* pref_service)
    : duration_tracker_observer_(this), pref_service_(pref_service) {
  AddDurationTrackerObserver();
}

SessionDurationUpdater::~SessionDurationUpdater() = default;

// static
void SessionDurationUpdater::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kObservedSessionTime, 0);
}

void SessionDurationUpdater::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);

  // Re-adds SessionDurationUpdater as an observer of
  // DesktopSessionDurationTracker if another feature is added after
  // SessionDurationUpdater was removed.
  if (!duration_tracker_observer_.IsObserving(
          metrics::DesktopSessionDurationTracker::Get()))
    AddDurationTrackerObserver();
}

void SessionDurationUpdater::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
  // If all the observer Features have removed themselves due to their active
  // time limits have been reached, the SessionDurationUpdater removes itself
  // as an observer of DesktopSessionDurationTracker.
  if (!observer_list_.might_have_observers())
    RemoveDurationTrackerObserver();
}

void SessionDurationUpdater::OnSessionEnded(base::TimeDelta elapsed) {
  // This case is only used during testing as that is the only case that
  // DesktopSessionDurationTracker isn't calling this on its observer.
  if (!duration_tracker_observer_.IsObserving(
          metrics::DesktopSessionDurationTracker::Get())) {
    return;
  }

  base::TimeDelta elapsed_session_time;

  elapsed_session_time +=
      base::TimeDelta::FromMinutes(
          pref_service_->GetInteger(prefs::kObservedSessionTime)) +
      elapsed;
  pref_service_->SetInteger(prefs::kObservedSessionTime,
                            elapsed_session_time.InMinutes());

  for (Observer& observer : observer_list_)
    observer.OnSessionEnded(elapsed_session_time);
}

void SessionDurationUpdater::AddDurationTrackerObserver() {
  duration_tracker_observer_.Add(metrics::DesktopSessionDurationTracker::Get());
}

void SessionDurationUpdater::RemoveDurationTrackerObserver() {
  duration_tracker_observer_.Remove(
      metrics::DesktopSessionDurationTracker::Get());
}

PrefService* SessionDurationUpdater::GetPrefs() {
  return pref_service_;
}

}  // namespace feature_engagement
