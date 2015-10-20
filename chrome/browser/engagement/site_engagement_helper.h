// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
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
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void WasShown() override;
  void WasHidden() override;

 private:
  // Class to encapsulate the user input listening.
  //
  // Time on site is recorded by detecting any user input (mouse or keypress)
  // per some discrete time unit. If there is user input, then record a positive
  // site engagement.
  //
  // When input is detected, SiteEngagementHelper::RecordUserInput is called,
  // and the input detection callbacks are paused for
  // g_seconds_between_user_input_check. This ensures that there is minimal
  // overhead in input listening, and that input over an extended length of time
  // is required to continually increase the engagement score.
  class InputTracker {
   public:
    explicit InputTracker(SiteEngagementHelper* helper);
    ~InputTracker();

    // Callback to handle key press events from the RenderViewHost.
    bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

    // Callback to handle mouse events from the RenderViewHost.
    bool HandleMouseEvent(const blink::WebMouseEvent& event);

    // Begin tracking input after |initial_delay|.
    void Start(content::RenderViewHost* host, base::TimeDelta initial_delay);

    // Pause listening for user input, restarting listening after a delay.
    void Pause();

    // Switches the InputTracker to another RenderViewHost, respecting the pause
    // timer state.
    void SwitchRenderViewHost(content::RenderViewHost* old_host,
                              content::RenderViewHost* new_host);

    // Stop listening for user input.
    void Stop();

    // Returns whether the InputTracker has been started for a RenderViewHost.
    bool IsActive() const;

    // Returns whether input tracking callbacks have been added to
    // RenderViewHost.
    bool is_tracking() const { return is_tracking_; }

    content::RenderViewHost* host() const { return host_; }

    // Set the timer object for testing purposes.
    void SetPauseTimerForTesting(scoped_ptr<base::Timer> timer);

   private:
    // Starts the timer for adding callbacks to the RenderViewHost.
    void StartTimer(base::TimeDelta delay);

    // Adds/removes tracking callbacks to the RenderViewHost.
    void AddCallbacks();
    void RemoveCallbacks();

    SiteEngagementHelper* helper_;
    scoped_ptr<base::Timer> pause_timer_;
    content::RenderViewHost* host_;
    content::RenderWidgetHost::KeyPressEventCallback key_press_event_callback_;
    content::RenderWidgetHost::MouseEventCallback mouse_event_callback_;
    bool is_tracking_;
  };

  explicit SiteEngagementHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SiteEngagementHelper>;
  friend class SiteEngagementServiceBrowserTest;

  // Ask the SiteEngagementService to record engagement via user input at the
  // current contents location.
  void RecordUserInput(SiteEngagementMetrics::EngagementType type);

  InputTracker input_tracker_;
  bool record_engagement_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementHelper);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_
