// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_ENGAGEMENT_DESKTOP_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_METRICS_DESKTOP_ENGAGEMENT_DESKTOP_ENGAGEMENT_SERVICE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/metrics/desktop_engagement/audible_contents_tracker.h"
#include "chrome/browser/metrics/desktop_engagement/chrome_visibility_observer.h"

namespace metrics {

// Class for tracking and recording user engagement based on browser visibility,
// audio and user interaction.
class DesktopEngagementService : public AudibleContentsTracker::Observer {
 public:
  // Creates the |DesktopEngagementService| instance and initializes the
  // observes that notify to it.
  static void Initialize();

  // Returns true if the |DesktopEngagementService| instance has been created.
  static bool IsInitialized();

  // Returns the |DesktopEngagementService| instance.
  static DesktopEngagementService* Get();

  // Called when user interaction with the browser is caught.
  void OnUserEvent();

  // Called when visibility of the browser changes.
  void OnVisibilityChanged(bool visible);

  bool is_visible() const { return is_visible_; }
  bool in_session() const { return in_session_; }
  bool is_audio_playing() const { return is_audio_playing_; }

  void SetInactivityTimeoutForTesting(int seconds) {
    inactivity_timeout_ = base::TimeDelta::FromSeconds(seconds);
  }

 protected:
  DesktopEngagementService();
  ~DesktopEngagementService() override;

  // AudibleContentsTracker::Observer
  void OnAudioStart() override;
  void OnAudioEnd() override;

  // Decides whether session should be ended. Called when timer for inactivity
  // timeout was fired. Overridden by tests.
  virtual void OnTimerFired();

 private:
  // Starts timer based on |inactivity_timeout_|.
  void StartTimer(base::TimeDelta duration);

  // Marks the start of the session.
  void StartSession();

  // Ends the session and saves session information into histograms.
  void EndSession();

  // Sets |inactivity_timeout_| based on variation params.
  void InitInactivityTimeout();

  // Used for marking start if the session.
  base::TimeTicks session_start_;

  // Used for marking last user interaction.
  base::TimeTicks last_user_event_;

  // Used for marking current state of the user engagement.
  bool is_visible_ = false;
  bool in_session_ = false;
  bool is_audio_playing_ = false;
  bool is_first_session_ = true;

  // Timeout for waiting for user interaction.
  base::TimeDelta inactivity_timeout_;

  base::OneShotTimer timer_;

  ChromeVisibilityObserver visibility_observer_;
  AudibleContentsTracker audio_tracker_;

  base::WeakPtrFactory<DesktopEngagementService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopEngagementService);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_ENGAGEMENT_DESKTOP_ENGAGEMENT_SERVICE_H_
