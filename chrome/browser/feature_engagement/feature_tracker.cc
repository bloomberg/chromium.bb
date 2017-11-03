// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/feature_tracker.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {

FeatureTracker::FeatureTracker(
    Profile* profile,
    const base::Feature* feature,
    const char* observed_session_time_dict_key,
    base::TimeDelta default_time_required_to_show_promo)
    : profile_(profile),
      session_duration_updater_(profile->GetPrefs(),
                                observed_session_time_dict_key),
      session_duration_observer_(this),
      feature_(feature),
      field_trial_time_delta_(default_time_required_to_show_promo) {
  if (!HasEnoughSessionTimeElapsed(
          session_duration_updater_.GetCumulativeElapsedSessionTime())) {
    AddSessionDurationObserver();
  }
}

FeatureTracker::~FeatureTracker() = default;

void FeatureTracker::AddSessionDurationObserver() {
  session_duration_observer_.Add(&session_duration_updater_);
}

void FeatureTracker::RemoveSessionDurationObserver() {
  session_duration_observer_.Remove(&session_duration_updater_);
}

bool FeatureTracker::IsObserving() {
  return session_duration_observer_.IsObserving(&session_duration_updater_);
}

bool FeatureTracker::ShouldShowPromo() {
  if (IsObserving()) {
    NotifyAndRemoveSessionDurationObserverIfSessionTimeMet(
        session_duration_updater_.GetCumulativeElapsedSessionTime());
  }

  return GetTracker()->ShouldTriggerHelpUI(*feature_);
}

Tracker* FeatureTracker::GetTracker() const {
  return TrackerFactory::GetForBrowserContext(profile_);
}

void FeatureTracker::OnSessionEnded(base::TimeDelta total_session_time) {
  NotifyAndRemoveSessionDurationObserverIfSessionTimeMet(total_session_time);
}

base::TimeDelta FeatureTracker::GetSessionTimeRequiredToShow() {
  if (!has_retrieved_field_trial_minutes_) {
    has_retrieved_field_trial_minutes_ = true;
    std::string field_trial_string_value =
        base::GetFieldTrialParamValueByFeature(*feature_, "x_minutes");
    int field_trial_int_value;
    if (base::StringToInt(field_trial_string_value, &field_trial_int_value)) {
      field_trial_time_delta_ =
          base::TimeDelta::FromMinutes(field_trial_int_value);
    }
  }
  return field_trial_time_delta_;
}

void FeatureTracker::NotifyAndRemoveSessionDurationObserverIfSessionTimeMet(
    base::TimeDelta total_session_time) {
  if (has_session_time_been_met_ ||
      !HasEnoughSessionTimeElapsed(total_session_time)) {
    return;
  }

  has_session_time_been_met_ = true;
  OnSessionTimeMet();
  RemoveSessionDurationObserver();
}

bool FeatureTracker::HasEnoughSessionTimeElapsed(
    base::TimeDelta total_session_time) {
  return total_session_time.InSeconds() >=
         GetSessionTimeRequiredToShow().InSeconds();
}

}  // namespace feature_engagement
