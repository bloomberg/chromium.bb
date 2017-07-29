// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"

#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

const int kTwoHoursInMinutes = 120;

}  // namespace

namespace feature_engagement {

NewTabTracker::NewTabTracker(Profile* profile)
    : profile_(profile), duration_tracker_observer_(this) {
  AddDurationTrackerObserver();
}

NewTabTracker::NewTabTracker() : NewTabTracker(nullptr) {}

NewTabTracker::~NewTabTracker() = default;

// static
void NewTabTracker::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kNewTabInProductHelp, false);
  registry->RegisterIntegerPref(prefs::kSessionTimeTotal, 0);
}

void NewTabTracker::DismissNewTabTracker() {
  GetFeatureTracker()->Dismissed(kIPHNewTabFeature);
}

void NewTabTracker::OnNewTabOpened() {
  GetFeatureTracker()->NotifyEvent(events::kNewTabOpened);
}

void NewTabTracker::OnOmniboxNavigation() {
  GetFeatureTracker()->NotifyEvent(events::kOmniboxInteraction);
}

void NewTabTracker::OnSessionTimeMet() {
  GetFeatureTracker()->NotifyEvent(events::kSessionTime);
}

void NewTabTracker::OnOmniboxFocused() {
  if (ShouldShowPromo())
    ShowPromo();
}

bool NewTabTracker::ShouldShowPromo() {
  return GetFeatureTracker()->ShouldTriggerHelpUI(kIPHNewTabFeature);
}

void NewTabTracker::AddDurationTrackerObserver() {
  duration_tracker_observer_.Add(metrics::DesktopSessionDurationTracker::Get());
}

void NewTabTracker::RemoveDurationTrackerObserver() {
  duration_tracker_observer_.Remove(
      metrics::DesktopSessionDurationTracker::Get());
}

bool NewTabTracker::HasEnoughSessionTimeElapsed() {
  return GetPrefs()->GetInteger(prefs::kSessionTimeTotal) >= kTwoHoursInMinutes;
}

void NewTabTracker::ShowPromo() {
  GetPrefs()->SetBoolean(prefs::kNewTabInProductHelp, true);
  // TODO(crbug.com/737830): Call the promo.

  // Clears the flag for whether there is any in-product help being displayed.
  GetFeatureTracker()->Dismissed(kIPHNewTabFeature);
}

Tracker* NewTabTracker::GetFeatureTracker() {
  return TrackerFactory::GetForBrowserContext(profile_);
}

PrefService* NewTabTracker::GetPrefs() {
  return profile_->GetPrefs();
}

void NewTabTracker::UpdateSessionTime(base::TimeDelta elapsed) {
  // Session time does not need to be tracked anymore if the
  // in-product help has been shown already.
  // This prevents unnecessary interaction with prefs.
  if (GetPrefs()->GetBoolean(prefs::kNewTabInProductHelp))
    return;

  base::TimeDelta elapsed_session_time;
  elapsed_session_time += base::TimeDelta::FromMinutes(GetPrefs()->GetInteger(
                              prefs::kSessionTimeTotal)) +
                          elapsed;
  GetPrefs()->SetInteger(prefs::kSessionTimeTotal,
                         elapsed_session_time.InMinutes());
}

void NewTabTracker::OnSessionEnded(base::TimeDelta delta) {
  UpdateSessionTime(delta);
  if (HasEnoughSessionTimeElapsed()) {
    OnSessionTimeMet();
    RemoveDurationTrackerObserver();
  }
}

}  // namespace feature_engagement
