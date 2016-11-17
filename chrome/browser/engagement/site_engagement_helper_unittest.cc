// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_helper.h"

#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class SiteEngagementHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SiteEngagementScore::SetParamValuesForTesting();
  }

  SiteEngagementService::Helper* GetHelper(content::WebContents* web_contents) {
    SiteEngagementService::Helper::CreateForWebContents(web_contents);
    SiteEngagementService::Helper* helper =
        SiteEngagementService::Helper::FromWebContents(web_contents);

    DCHECK(helper);
    return helper;
  }

  void TrackingStarted(SiteEngagementService::Helper* helper) {
    helper->input_tracker_.TrackingStarted();
    helper->media_tracker_.TrackingStarted();
  }

  // Simulate a user interaction event and handle it.
  void HandleUserInput(SiteEngagementService::Helper* helper,
                       blink::WebInputEvent::Type type) {
    helper->input_tracker_.DidGetUserInteraction(type);
  }

  // Simulate a user interaction event and handle it. Reactivates tracking
  // immediately.
  void HandleUserInputAndRestartTracking(SiteEngagementService::Helper* helper,
                                         blink::WebInputEvent::Type type) {
    helper->input_tracker_.DidGetUserInteraction(type);
    helper->input_tracker_.TrackingStarted();
  }

  void HandleMediaPlaying(SiteEngagementService::Helper* helper,
                          bool is_hidden) {
    helper->RecordMediaPlaying(is_hidden);
  }

  void MediaStartedPlaying(SiteEngagementService::Helper* helper) {
    helper->media_tracker_.MediaStartedPlaying(
        content::WebContentsObserver::MediaPlayerInfo(false),
        content::WebContentsObserver::MediaPlayerId(nullptr, 1));
  }

  void MediaStoppedPlaying(SiteEngagementService::Helper* helper) {
    helper->media_tracker_.MediaStoppedPlaying(
        content::WebContentsObserver::MediaPlayerInfo(false),
        content::WebContentsObserver::MediaPlayerId(nullptr, 1));
  }

  // Set a pause timer on the input tracker for test purposes.
  void SetInputTrackerPauseTimer(SiteEngagementService::Helper* helper,
                                 std::unique_ptr<base::Timer> timer) {
    helper->input_tracker_.SetPauseTimerForTesting(std::move(timer));
  }

  // Set a pause timer on the input tracker for test purposes.
  void SetMediaTrackerPauseTimer(SiteEngagementService::Helper* helper,
                                 std::unique_ptr<base::Timer> timer) {
    helper->media_tracker_.SetPauseTimerForTesting(std::move(timer));
  }

  bool IsTrackingInput(SiteEngagementService::Helper* helper) {
    return helper->input_tracker_.is_tracking();
  }

  void Navigate(const GURL& url) {
    controller().LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                         std::string());
    int pending_id = controller().GetPendingEntry()->GetUniqueID();
    content::WebContentsTester::For(web_contents())
        ->TestDidNavigate(web_contents()->GetMainFrame(), pending_id, true,
                          url, ui::PAGE_TRANSITION_TYPED);
  }

  void UserInputAccumulation(const blink::WebInputEvent::Type type) {
    GURL url1("https://www.google.com/");
    GURL url2("http://www.google.com/");
    content::WebContents* contents = web_contents();

    SiteEngagementService::Helper* helper = GetHelper(contents);
    SiteEngagementService* service = SiteEngagementService::Get(profile());
    DCHECK(service);

    // Check that navigation triggers engagement.
    Navigate(url1);
    TrackingStarted(helper);

    EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));

    // Simulate a user input trigger and ensure it is treated correctly.
    HandleUserInputAndRestartTracking(helper, type);

    EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));

    // Simulate three inputs , and ensure they are treated correctly.
    HandleUserInputAndRestartTracking(helper, type);
    HandleUserInputAndRestartTracking(helper, type);
    HandleUserInputAndRestartTracking(helper, type);

    EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));

    // Simulate inputs for a different link.
    Navigate(url2);
    TrackingStarted(helper);

    EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
    EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
    EXPECT_DOUBLE_EQ(1.2, service->GetTotalEngagementPoints());

    HandleUserInputAndRestartTracking(helper, type);
    EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
    EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
    EXPECT_DOUBLE_EQ(1.25, service->GetTotalEngagementPoints());
  }
};

TEST_F(SiteEngagementHelperTest, KeyPressEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::RawKeyDown);
}

TEST_F(SiteEngagementHelperTest, MouseDownEventEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::MouseDown);
}

TEST_F(SiteEngagementHelperTest, ScrollEventEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::GestureScrollBegin);
}

TEST_F(SiteEngagementHelperTest, TouchEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::TouchStart);
}

TEST_F(SiteEngagementHelperTest, MediaEngagementAccumulation) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  SiteEngagementService::Helper* helper = GetHelper(contents);
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  DCHECK(service);

  Navigate(url1);
  TrackingStarted(helper);

  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate a foreground media input and ensure it is treated correctly.
  HandleMediaPlaying(helper, false);

  EXPECT_DOUBLE_EQ(0.52, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate continual media playing, and ensure it is treated correctly.
  HandleMediaPlaying(helper, false);
  HandleMediaPlaying(helper, false);
  HandleMediaPlaying(helper, false);

  EXPECT_DOUBLE_EQ(0.58, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate backgrounding the media.
  HandleMediaPlaying(helper, true);
  HandleMediaPlaying(helper, true);

  EXPECT_DOUBLE_EQ(0.60, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate inputs for a different link.
  Navigate(url2);
  TrackingStarted(helper);

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.1, service->GetTotalEngagementPoints());

  HandleMediaPlaying(helper, false);
  HandleMediaPlaying(helper, false);
  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.54, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.14, service->GetTotalEngagementPoints());
}

TEST_F(SiteEngagementHelperTest, MediaEngagement) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  base::MockTimer* media_tracker_timer = new base::MockTimer(true, false);
  SiteEngagementService::Helper* helper = GetHelper(contents);
  SetMediaTrackerPauseTimer(helper, base::WrapUnique(media_tracker_timer));
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  DCHECK(service);

  Navigate(url1);
  MediaStartedPlaying(helper);

  EXPECT_DOUBLE_EQ(0.50, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.52, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  contents->WasHidden();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.53, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  MediaStoppedPlaying(helper);
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.53, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  contents->WasShown();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.53, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  MediaStartedPlaying(helper);
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  Navigate(url2);
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.5, service->GetScore(url2));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  MediaStartedPlaying(helper);
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.52, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  contents->WasHidden();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.53, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  MediaStoppedPlaying(helper);
  contents->WasShown();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.53, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());
}

TEST_F(SiteEngagementHelperTest, MixedInputEngagementAccumulation) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  SiteEngagementService::Helper* helper = GetHelper(contents);
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  DCHECK(service);

  base::HistogramTester histograms;

  // Histograms should start off empty.
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              0);

  Navigate(url1);
  TrackingStarted(helper);

  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 1);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 1);

  HandleUserInputAndRestartTracking(helper, blink::WebInputEvent::RawKeyDown);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::TouchStart);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::TouchStart);
  HandleUserInputAndRestartTracking(helper, blink::WebInputEvent::RawKeyDown);
  HandleUserInputAndRestartTracking(helper, blink::WebInputEvent::MouseDown);

  EXPECT_DOUBLE_EQ(0.75, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              7);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE,
                               2);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 1);

  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::GestureScrollBegin);
  HandleUserInputAndRestartTracking(helper, blink::WebInputEvent::MouseDown);
  HandleMediaPlaying(helper, true);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::TouchStart);
  HandleMediaPlaying(helper, false);

  EXPECT_DOUBLE_EQ(0.93, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              12);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_SCROLL, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE,
                               3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_VISIBLE,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_HIDDEN,
                               1);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 1);

  Navigate(url2);
  TrackingStarted(helper);

  EXPECT_DOUBLE_EQ(0.93, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.43, service->GetTotalEngagementPoints());

  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::TouchStart);
  HandleUserInputAndRestartTracking(helper, blink::WebInputEvent::RawKeyDown);

  EXPECT_DOUBLE_EQ(0.93, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.53, service->GetTotalEngagementPoints());
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              16);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE,
                               4);
  histograms.ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT, 2);
}

TEST_F(SiteEngagementHelperTest, CheckTimerAndCallbacks) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  base::MockTimer* media_tracker_timer = new base::MockTimer(true, false);
  SiteEngagementService::Helper* helper = GetHelper(contents);
  SetInputTrackerPauseTimer(helper, base::WrapUnique(input_tracker_timer));
  SetMediaTrackerPauseTimer(helper, base::WrapUnique(media_tracker_timer));

  SiteEngagementService* service = SiteEngagementService::Get(profile());
  DCHECK(service);

  Navigate(url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Input timer should be running for navigation delay, but media timer is
  // inactive.
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  // Media timer starts once media is detected as playing.
  MediaStartedPlaying(helper);
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  input_tracker_timer->Fire();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.52, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Input timer should start running again after input, but the media timer
  // keeps running.
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  HandleUserInput(helper, blink::WebInputEvent::RawKeyDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.57, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  // Timer should start running again after input.
  HandleUserInput(helper, blink::WebInputEvent::TouchStart);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.62, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));

  media_tracker_timer->Fire();
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Timer should be running for navigation delay. Media is disabled again.
  Navigate(url2);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.14, service->GetTotalEngagementPoints());

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  HandleUserInput(helper, blink::WebInputEvent::MouseDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  MediaStartedPlaying(helper);
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.19, service->GetTotalEngagementPoints());

  EXPECT_TRUE(media_tracker_timer->IsRunning());
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.57, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.21, service->GetTotalEngagementPoints());
}

// Ensure that navigation and tab activation/hiding does not trigger input
// tracking until after a delay. We must manually call WasShown/WasHidden as
// they are not triggered automatically in this test environment.
TEST_F(SiteEngagementHelperTest, ShowAndHide) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  base::MockTimer* media_tracker_timer = new base::MockTimer(true, false);
  SiteEngagementService::Helper* helper = GetHelper(contents);
  SetInputTrackerPauseTimer(helper, base::WrapUnique(input_tracker_timer));
  SetMediaTrackerPauseTimer(helper, base::WrapUnique(media_tracker_timer));

  Navigate(url1);
  input_tracker_timer->Fire();

  // Hiding the tab should stop input tracking. Media tracking remains inactive.
  contents->WasHidden();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // Showing the tab should start tracking again after another delay. Media
  // tracking remains inactive.
  contents->WasShown();
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // Start media tracking.
  MediaStartedPlaying(helper);
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  // Hiding the tab should stop input tracking, but not media tracking.
  contents->WasHidden();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // Showing the tab should start tracking again after another delay. Media
  // tracking continues.
  contents->WasShown();
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  input_tracker_timer->Fire();
  media_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));
}

// Ensure tracking behavior is correct for multiple navigations in a single tab.
TEST_F(SiteEngagementHelperTest, SingleTabNavigation) {
  GURL url1("https://www.google.com/");
  GURL url2("https://www.example.com/");
  content::WebContents* contents = web_contents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  SiteEngagementService::Helper* helper = GetHelper(contents);
  SetInputTrackerPauseTimer(helper, base::WrapUnique(input_tracker_timer));

  // Navigation should start the initial delay timer.
  Navigate(url1);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // Navigating before the timer fires should simply reset the timer.
  Navigate(url2);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // When the timer fires, callbacks are added.
  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));

  // Navigation should start the initial delay timer again.
  Navigate(url1);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
}
