// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/recommendation_restorer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {
// The amount of idle time after which recommended values are restored.
const int kRestoreDelayInMs = 60 * 1000;  // 1 minute.
}  // namespace

RecommendationRestorer::RecommendationRestorer(Profile* profile)
    : logged_in_(false) {
  if (!chromeos::ProfileHelper::IsSigninProfile(profile))
    return;

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kAccessibilityLargeCursorEnabled,
      base::Bind(
          &RecommendationRestorer::Restore, base::Unretained(this), true));
  pref_change_registrar_.Add(
      prefs::kAccessibilitySpokenFeedbackEnabled,
      base::Bind(
          &RecommendationRestorer::Restore, base::Unretained(this), true));
  pref_change_registrar_.Add(
      prefs::kAccessibilityHighContrastEnabled,
      base::Bind(
          &RecommendationRestorer::Restore, base::Unretained(this), true));
  pref_change_registrar_.Add(
      prefs::kAccessibilityScreenMagnifierEnabled,
      base::Bind(
          &RecommendationRestorer::Restore, base::Unretained(this), true));
  pref_change_registrar_.Add(
      prefs::kAccessibilityScreenMagnifierType,
      base::Bind(
          &RecommendationRestorer::Restore, base::Unretained(this), true));
  pref_change_registrar_.Add(
      prefs::kAccessibilityVirtualKeyboardEnabled,
      base::Bind(
          &RecommendationRestorer::Restore, base::Unretained(this), true));

  notification_registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                              content::NotificationService::AllSources());

  RestoreAll();
}

RecommendationRestorer::~RecommendationRestorer() {
}

void RecommendationRestorer::Shutdown() {
  StopTimer();
  pref_change_registrar_.RemoveAll();
  notification_registrar_.RemoveAll();
}

void RecommendationRestorer::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    logged_in_ = true;
    notification_registrar_.RemoveAll();
    StopTimer();
    RestoreAll();
  } else {
    NOTREACHED();
  }
}

void RecommendationRestorer::OnUserActivity(const ui::Event* event) {
  if (restore_timer_.IsRunning())
    restore_timer_.Reset();
}

void RecommendationRestorer::Restore(bool allow_delay,
                                     const std::string& pref_name) {
  const PrefService::Preference* pref =
      pref_change_registrar_.prefs()->FindPreference(pref_name.c_str());
  if (!pref) {
    NOTREACHED();
    return;
  }

  if (!pref->GetRecommendedValue() || !pref->HasUserSetting())
    return;

  if (logged_in_) {
    allow_delay = false;
  } else if (allow_delay) {
    // Skip the delay if there has been no user input since the browser started.
    const ui::UserActivityDetector* user_activity_detector =
        ui::UserActivityDetector::Get();
    if (user_activity_detector &&
        user_activity_detector->last_activity_time().is_null()) {
      allow_delay = false;
    }
  }

  if (allow_delay)
    StartTimer();
  else
    pref_change_registrar_.prefs()->ClearPref(pref->name().c_str());
}

void RecommendationRestorer::RestoreAll() {
  Restore(false, prefs::kAccessibilityLargeCursorEnabled);
  Restore(false, prefs::kAccessibilitySpokenFeedbackEnabled);
  Restore(false, prefs::kAccessibilityHighContrastEnabled);
  Restore(false, prefs::kAccessibilityScreenMagnifierEnabled);
  Restore(false, prefs::kAccessibilityScreenMagnifierType);
  Restore(false, prefs::kAccessibilityVirtualKeyboardEnabled);
}

void RecommendationRestorer::StartTimer() {
  // Listen for user activity so that the timer can be reset while the user is
  // active, causing it to fire only when the user remains idle for
  // |kRestoreDelayInMs|.
  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  if (user_activity_detector && !user_activity_detector->HasObserver(this))
    user_activity_detector->AddObserver(this);

  // There should be a separate timer for each pref. However, in the common
  // case of the user changing settings, a single timer is sufficient. This is
  // because a change initiated by the user implies user activity, so that even
  // if there was a separate timer per pref, they would all be reset at that
  // point, causing them to fire at exactly the same time. In the much rarer
  // case of a recommended value changing, a single timer is a close
  // approximation of the behavior that would be obtained by resetting the timer
  // for the affected pref only.
  restore_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kRestoreDelayInMs),
                       base::Bind(&RecommendationRestorer::RestoreAll,
                                  base::Unretained(this)));
}

void RecommendationRestorer::StopTimer() {
  restore_timer_.Stop();
  if (ui::UserActivityDetector::Get())
    ui::UserActivityDetector::Get()->RemoveObserver(this);
}

}  // namespace policy
