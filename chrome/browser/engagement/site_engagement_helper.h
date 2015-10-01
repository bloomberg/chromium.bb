// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "content/public/browser/render_view_host.h"
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

  static void SetSecondsBetweenUserInputCheck(double seconds);

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

    // Register callbacks to listen for user input.
    void StartTracking(content::RenderViewHost* host);

    // Pause listening for user input, restarting listening after
    // g_seconds_between_user_input_check seconds.
    void PauseTracking(content::RenderViewHost* host);

    // Restart listening for user input.
    void ResumeTracking();

    // Stop listening for user input.
    void StopTracking(content::RenderViewHost* host);

    // Set the timer object for testing purposes.
    void SetTimerForTesting(scoped_ptr<base::Timer> timer);

    bool callbacks_added() { return callbacks_added_; }

   private:
    SiteEngagementHelper* helper_;
    scoped_ptr<base::Timer> pause_timer_;
    content::RenderWidgetHost::KeyPressEventCallback key_press_event_callback_;
    content::RenderWidgetHost::MouseEventCallback mouse_event_callback_;
    bool callbacks_added_;
  };

  explicit SiteEngagementHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SiteEngagementHelper>;
  friend class SiteEngagementServiceBrowserTest;

  // Ask the SiteEngagementService to record engagement via user input at the
  // current contents location.
  void RecordUserInput();

  bool ShouldRecordEngagement();

  // content::WebContentsObserver overrides.
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  void WasShown() override;
  void WasHidden() override;

  InputTracker input_tracker_;
  bool record_engagement_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementHelper);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_
