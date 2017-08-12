// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/feature_tracker.h"

#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {

FeatureTracker::FeatureTracker(Profile* profile,
                               SessionDurationUpdater* session_duration_updater)
    : profile_(profile),
      session_duration_updater_(session_duration_updater),
      session_duration_observer_(this) {
  AddSessionDurationObserver();
}

FeatureTracker::~FeatureTracker() = default;

void FeatureTracker::AddSessionDurationObserver() {
  session_duration_observer_.Add(session_duration_updater_);
}

void FeatureTracker::RemoveSessionDurationObserver() {
  session_duration_observer_.Remove(session_duration_updater_);
}

bool FeatureTracker::IsObserving() {
  return session_duration_observer_.IsObserving(session_duration_updater_);
}

Tracker* FeatureTracker::GetTracker() const {
  return TrackerFactory::GetForBrowserContext(profile_);
}

void FeatureTracker::OnSessionEnded(base::TimeDelta total_session_time) {
  if (HasEnoughSessionTimeElapsed(total_session_time)) {
    OnSessionTimeMet();
    RemoveSessionDurationObserver();
  }
}

bool FeatureTracker::HasEnoughSessionTimeElapsed(
    base::TimeDelta total_session_time) {
  return total_session_time.InMinutes() >=
         GetSessionTimeRequiredToShowInMinutes();
}

}  // namespace feature_engagement
