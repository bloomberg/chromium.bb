// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker.h"

#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

namespace {

constexpr int kDefaultPromoShowTimeInHours = 2;

}  // namespace

namespace feature_engagement {

IncognitoWindowTracker::IncognitoWindowTracker(
    Profile* profile,
    SessionDurationUpdater* session_duration_updater)
    : FeatureTracker(profile,
                     session_duration_updater,
                     &kIPHIncognitoWindowFeature,
                     base::TimeDelta::FromHours(kDefaultPromoShowTimeInHours)) {
}

IncognitoWindowTracker::IncognitoWindowTracker(
    SessionDurationUpdater* session_duration_updater)
    : IncognitoWindowTracker(nullptr, session_duration_updater) {}

IncognitoWindowTracker::~IncognitoWindowTracker() = default;

void IncognitoWindowTracker::OnIncognitoWindowOpened() {
  GetTracker()->NotifyEvent(events::kIncognitoWindowOpened);
}

void IncognitoWindowTracker::OnBrowsingDataCleared() {
  if (ShouldShowPromo())
    ShowPromo();
}

void IncognitoWindowTracker::OnPromoClosed() {
  GetTracker()->Dismissed(kIPHIncognitoWindowFeature);
}

void IncognitoWindowTracker::OnSessionTimeMet() {
  GetTracker()->NotifyEvent(events::kIncognitoWindowSessionTimeMet);
}

void IncognitoWindowTracker::ShowPromo() {
  // TODO: Call the promo.

  // Clears the flag for whether there is any in-product help being displayed.
  GetTracker()->Dismissed(kIPHIncognitoWindowFeature);
}

}  // namespace feature_engagement
