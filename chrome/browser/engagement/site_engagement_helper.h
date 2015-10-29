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

  // content::WebContentsObserver overrides.
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void WasShown() override;
  void WasHidden() override;

 private:
  // Class to encapsulate the user input listening.
  //
  // Time on site is recorded by detecting any user input (mouse click,
  // keypress, or touch gesture tap) per some discrete time unit. If there is
  // user input, then record a positive site engagement.
  //
  // When input is detected, SiteEngagementHelper::RecordUserInput is called,
  // and the input detection callbacks are paused for
  // g_seconds_between_user_input_check. This ensures that there is minimal
  // overhead in input listening, and that input over an extended length of time
  // is required to continually increase the engagement score.
  class InputTracker : public content::WebContentsObserver {
   public:
    explicit InputTracker(content::WebContents* web_contents,
                          SiteEngagementHelper* helper);
    ~InputTracker() override;

    // Begin tracking input after |initial_delay|.
    void Start(base::TimeDelta initial_delay);

    // Pause listening for user input, restarting listening after a delay.
    void Pause();

    // Stop listening for user input.
    void Stop();

    // Returns whether the tracker will respond to user input via
    // DidGetUserInteraction.
    bool is_tracking() const { return is_tracking_; }

    // Set the timer object for testing purposes.
    void SetPauseTimerForTesting(scoped_ptr<base::Timer> timer);

   private:
    friend class SiteEngagementHelperTest;

    // Starts the timer for detecting user interaction.
    void StartTimer(base::TimeDelta delay);

    // Callback for StartTimer that activates the user input tracking.
    void StartTracking();

    // content::WebContentsObserver overrides.
    void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;

    SiteEngagementHelper* helper_;
    scoped_ptr<base::Timer> pause_timer_;
    bool is_tracking_;
  };

  explicit SiteEngagementHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SiteEngagementHelper>;
  friend class SiteEngagementHelperTest;

  // Ask the SiteEngagementService to record engagement via user input at the
  // current contents location.
  void RecordUserInput(SiteEngagementMetrics::EngagementType type);

  InputTracker input_tracker_;
  bool record_engagement_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementHelper);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_
