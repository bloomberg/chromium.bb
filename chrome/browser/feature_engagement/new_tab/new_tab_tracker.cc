// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

namespace {

const int kTwoHoursInMinutes = 120;

}  // namespace

namespace feature_engagement {

NewTabTracker::NewTabTracker(Profile* profile,
                             SessionDurationUpdater* session_duration_updater)
    : FeatureTracker(profile, session_duration_updater) {}

NewTabTracker::NewTabTracker(SessionDurationUpdater* session_duration_updater)
    : NewTabTracker(nullptr, session_duration_updater) {}

NewTabTracker::~NewTabTracker() = default;

void NewTabTracker::OnNewTabOpened() {
  GetTracker()->NotifyEvent(events::kNewTabOpened);
}

void NewTabTracker::OnOmniboxNavigation() {
  GetTracker()->NotifyEvent(events::kOmniboxInteraction);
}

void NewTabTracker::OnOmniboxFocused() {
  if (ShouldShowPromo())
    ShowPromo();
}

void NewTabTracker::OnPromoClosed() {
  GetTracker()->Dismissed(kIPHNewTabFeature);
}

bool NewTabTracker::ShouldShowPromo() {
  return GetTracker()->ShouldTriggerHelpUI(kIPHNewTabFeature);
}

void NewTabTracker::OnSessionTimeMet() {
  GetTracker()->NotifyEvent(events::kNewTabSessionTimeMet);
}

int NewTabTracker::GetSessionTimeRequiredToShowInMinutes() {
  return kTwoHoursInMinutes;
}

void NewTabTracker::ShowPromo() {
  NewTabButton::ShowPromoForLastActiveBrowser();
}

}  // namespace feature_engagement
