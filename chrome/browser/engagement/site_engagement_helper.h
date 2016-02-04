// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class GURL;

// Per-WebContents class to handle updating the site engagement scores for
// origins.
class SiteEngagementHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SiteEngagementHelper> {
 public:
  ~SiteEngagementHelper() override;

  static void SetSecondsBetweenUserInputCheck(int seconds);
  static void SetSecondsTrackingDelayAfterNavigation(int seconds);
  static void SetSecondsTrackingDelayAfterShow(int seconds);

 private:
  // Class to encapsulate the periodic detection of site engagement.
  //
  // Engagement detection begins at some constant time delta following
  // navigation, tab activation, or media starting to play. Once engagement is
  // recorded, detection is suspended for another constant time delta. For sites
  // to continually record engagement, this overall design requires:
  //
  // 1. engagement at a non-trivial time after a site loads
  // 2. continual engagement over a non-trivial duration of time
  class PeriodicTracker {
   public:
    explicit PeriodicTracker(SiteEngagementHelper* helper);
    virtual ~PeriodicTracker();

    // Begin tracking after |initial_delay|.
    void Start(base::TimeDelta initial_delay);

    // Pause tracking and restart after a delay.
    void Pause();

    // Stop tracking.
    void Stop();

    // Returns true if the timer is currently running.
    bool IsTimerRunning();

    // Set the timer object for testing.
    void SetPauseTimerForTesting(scoped_ptr<base::Timer> timer);

    SiteEngagementHelper* helper() { return helper_; }

   protected:
    friend class SiteEngagementHelperTest;

    // Called when tracking is to be paused by |delay|. Used when tracking first
    // starts or is paused.
    void StartTimer(base::TimeDelta delay);

    // Called when the timer expires and engagement tracking is activated.
    virtual void TrackingStarted() {}

    // Called when engagement tracking is paused or stopped.
    virtual void TrackingStopped() {}

   private:
    SiteEngagementHelper* helper_;
    scoped_ptr<base::Timer> pause_timer_;
  };

  // Class to encapsulate time-on-site engagement detection. Time-on-site is
  // recorded by detecting user input on a focused WebContents (mouse click,
  // mouse wheel, keypress, or touch gesture tap) over time.
  //
  // After an initial delay, the input tracker begins listening to
  // DidGetUserInteraction. When user input is signaled, site engagement is
  // recorded, and the tracker sleeps for a delay period.
  class InputTracker : public PeriodicTracker,
                       public content::WebContentsObserver {
   public:
    InputTracker(SiteEngagementHelper* helper,
                 content::WebContents* web_contents);

    bool is_tracking() const { return is_tracking_; }

   private:
    friend class SiteEngagementHelperTest;

    void TrackingStarted() override;
    void TrackingStopped() override;

    // Returns whether the tracker will respond to user input via
    // DidGetUserInteraction.
    bool is_tracking_;

    // content::WebContentsObserver overrides.
    void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;
  };

  // Class to encapsulate media detection. Any media playing in a WebContents
  // (focused or not) will accumulate engagement points. Media in a hidden
  // WebContents will accumulate engagement more slowly than in an active
  // WebContents. Media which has been muted will also accumulate engagement
  // more slowly.
  //
  // When media begins playing in the main frame of a tab, the tracker is
  // triggered with an initial delay. It then wakes up every
  // |g_seconds_to_pause_engagement_detection| and notes the visible/hidden
  // state of the tab, as well as whether media is still playing.
  class MediaTracker : public PeriodicTracker,
                       public content::WebContentsObserver {
   public:
    MediaTracker(SiteEngagementHelper* helper,
                 content::WebContents* web_contents);
    ~MediaTracker() override;

   private:
    friend class SiteEngagementHelperTest;

    void TrackingStarted() override;

    // content::WebContentsObserver overrides.
    void MediaStartedPlaying(const MediaPlayerId& id) override;
    void MediaStoppedPlaying(const MediaPlayerId& id) override;
    void WasShown() override;
    void WasHidden() override;

    bool is_hidden_;
    std::vector<MediaPlayerId> active_media_players_;
  };

  explicit SiteEngagementHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SiteEngagementHelper>;
  friend class SiteEngagementHelperTest;

  // Ask the SiteEngagementService to record engagement via user input at the
  // current WebContents URL.
  void RecordUserInput(SiteEngagementMetrics::EngagementType type);

  // Ask the SiteEngagementService to record engagement via media playing at the
  // current WebContents URL.
  void RecordMediaPlaying(bool is_hidden);

  // content::WebContentsObserver overrides.
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void WasShown() override;
  void WasHidden() override;

  InputTracker input_tracker_;
  MediaTracker media_tracker_;
  bool record_engagement_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementHelper);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_
