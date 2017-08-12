// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_ENGAGEMENT_FEATURE_TRACKER_H_
#define CHROME_BROWSER_FEATURE_ENGAGEMENT_FEATURE_TRACKER_H_

#include "base/scoped_observer.h"
#include "chrome/browser/feature_engagement/session_duration_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

namespace feature_engagement {

class Tracker;

// The rFeatureTracker provides a backend for displaying in-product help for the
// various features by collecting all common funcitonality. All subclasses of
// FeatureTracker's factories depend on
// SessionDurationUpdaterFactory::GetInstance() as SessionDurationUpdater is
// responsible for letting all FeatureTrackers know how much active session time
// has passed.
//
// SessionDurationUpdater keeps track of the observed session time and, upon
// each session ending, updates all of the FeatureTrackers with the new total
// observed session time. Once the observed session time exceeds the time
// requirement provided by GetSessionTimeRequiredToShowInMinutes(), the
// FeatureTracker unregisters itself as an obsever of SessionDurationUpdater.
// SessionDurationUpdater stops updating the observed session time if no
// features are observing it, and will start tracking the observed sesion time
// again if another feature is added as an observer later.
class FeatureTracker : public SessionDurationUpdater::Observer,
                       public KeyedService {
 public:
  FeatureTracker(Profile* profile,
                 SessionDurationUpdater* session_duration_updater);

  // Adds the SessionDurationUpdater observer.
  void AddSessionDurationObserver();
  // Removes the SessionDurationUpdater observer.
  void RemoveSessionDurationObserver();

  // Returns the whether |session_duration_observer_| is observing sources for
  // testing purposes.
  bool IsObserving();

  // SessionDurationUpdater::Observer:
  void OnSessionEnded(base::TimeDelta total_session_time) override;

 protected:
  ~FeatureTracker() override;
  // Returns the required session time in minutes for the FeatureTracker's
  // subclass to show its promo.
  virtual int GetSessionTimeRequiredToShowInMinutes() = 0;
  // Alerts the feature tracker that the session time is up.
  virtual void OnSessionTimeMet() = 0;
  // Returns the Tracker associated with this FeatureTracker.
  virtual Tracker* GetTracker() const;

 private:
  // Returns whether the active session time of a user has elapsed more than the
  // required active session time for the feature.
  bool HasEnoughSessionTimeElapsed(base::TimeDelta total_session_time);

  // Owned by the ProfileManager.
  Profile* const profile_;

  // Singleton instance of KeyedService.
  SessionDurationUpdater* const session_duration_updater_;

  // Observes the SessionDurationUpdater and notifies when a desktop session
  // starts and ends.
  ScopedObserver<SessionDurationUpdater, SessionDurationUpdater::Observer>
      session_duration_observer_;

  DISALLOW_COPY_AND_ASSIGN(FeatureTracker);
};

}  // namespace feature_engagement

#endif  // CHROME_BROWSER_FEATURE_ENGAGEMENT_FEATURE_TRACKER_H_
