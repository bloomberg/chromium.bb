// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_ENGAGEMENT_NEW_TAB_NEW_TAB_TRACKER_H_
#define CHROME_BROWSER_FEATURE_ENGAGEMENT_NEW_TAB_NEW_TAB_TRACKER_H_

#include "base/scoped_observer.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace feature_engagement {

// The NewTabTracker provides a backend for displaying
// in-product help for the new tab button.
class NewTabTracker : public metrics::DesktopSessionDurationTracker::Observer,
                      public KeyedService {
 public:
  explicit NewTabTracker(Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Clears the flag for whether there is any in-product help being displayed.
  void DismissNewTabTracker();
  // Alerts the new tab tracker that a new tab was opened.
  void OnNewTabOpened();
  // Alerts the new tab tracker that the omnibox has been used.
  void OnOmniboxNavigation();
  // Alerts the new tab tracker that the session time is up.
  void OnSessionTimeMet();
  // Checks if the promo should be displayed since the omnibox is on focus.
  void OnOmniboxFocused();
  // Returns whether or not the promo should be displayed.
  bool ShouldShowPromo();

  // Adding the DesktopSessionDurationTracker observer.
  void AddDurationTrackerObserver();
  // Removing the DesktopSessionDurationTracker observer.
  void RemoveDurationTrackerObserver();

 protected:
  NewTabTracker();
  ~NewTabTracker() override;

 private:
  // Returns whether the active session time of a user has elapsed
  // more than two hours.
  bool HasEnoughSessionTimeElapsed();

  // Returns whether the NewTabInProductHelp field trial is enabled.
  bool IsIPHNewTabEnabled();

  // Sets the NewTabInProductHelp pref to true and calls the New Tab Promo.
  void ShowPromo();

  // Virtual to support mocking by unit tests.
  virtual Tracker* GetFeatureTracker();

  virtual PrefService* GetPrefs();

  // Updates the pref that stores active session time per user unless the
  // new tab in-product help has been displayed already.
  virtual void UpdateSessionTime(base::TimeDelta elapsed);

  // metrics::DesktopSessionDurationTracker::Observer::
  void OnSessionEnded(base::TimeDelta delta) override;

  // Owned by ProfileManager.
  Profile* const profile_;

  // Observes the DesktopSessionDurationTracker and notifies when a desktop
  // session starts and ends.
  ScopedObserver<metrics::DesktopSessionDurationTracker,
                 metrics::DesktopSessionDurationTracker::Observer>
      duration_tracker_observer_;

  DISALLOW_COPY_AND_ASSIGN(NewTabTracker);
};

}  // namespace feature_engagement

#endif  // CHROME_BROWSER_FEATURE_ENGAGEMENT_NEW_TAB_NEW_TAB_TRACKER_H_
