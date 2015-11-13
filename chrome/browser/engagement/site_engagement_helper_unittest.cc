// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/page_navigator.h"
#include "testing/gtest/include/gtest/gtest.h"

class SiteEngagementHelperTest : public BrowserWithTestWindowTest {
 public:
  // Create a SiteEngagementHelper. Called here as friend class methods cannot
  // be called in tests.
  scoped_ptr<SiteEngagementHelper> CreateHelper(
      content::WebContents* web_contents) {
    scoped_ptr<SiteEngagementHelper> helper(
        new SiteEngagementHelper(web_contents));
    DCHECK(helper.get());

    return helper.Pass();
  }

  void TrackingStarted(SiteEngagementHelper* helper) {
    helper->input_tracker_.TrackingStarted();
    helper->media_tracker_.TrackingStarted();
  }

  // Simulate a user interaction event and handle it.
  void HandleUserInput(SiteEngagementHelper* helper,
                       blink::WebInputEvent::Type type) {
    helper->input_tracker_.DidGetUserInteraction(type);
  }

  // Simulate a user interaction event and handle it. Reactivates tracking
  // immediately.
  void HandleUserInputAndRestartTracking(SiteEngagementHelper* helper,
                                         blink::WebInputEvent::Type type) {
    helper->input_tracker_.DidGetUserInteraction(type);
    helper->input_tracker_.TrackingStarted();
  }

  void HandleMediaPlaying(SiteEngagementHelper* helper, bool is_hidden) {
    helper->RecordMediaPlaying(is_hidden);
  }

  void MediaStartedPlaying(SiteEngagementHelper* helper) {
    helper->media_tracker_.MediaStartedPlaying();
  }

  void MediaPaused(SiteEngagementHelper* helper) {
    helper->media_tracker_.MediaPaused();
  }

  // Set a pause timer on the input tracker for test purposes.
  void SetInputTrackerPauseTimer(SiteEngagementHelper* helper,
                                 scoped_ptr<base::Timer> timer) {
    helper->input_tracker_.SetPauseTimerForTesting(timer.Pass());
  }

  // Set a pause timer on the input tracker for test purposes.
  void SetMediaTrackerPauseTimer(SiteEngagementHelper* helper,
                                 scoped_ptr<base::Timer> timer) {
    helper->media_tracker_.SetPauseTimerForTesting(timer.Pass());
  }

  bool IsTrackingInput(SiteEngagementHelper* helper) {
    return helper->input_tracker_.is_tracking();
  }

  void NavigateWithDisposition(GURL& url, WindowOpenDisposition disposition) {
    content::NavigationController* controller =
        &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
    browser()->OpenURL(
        content::OpenURLParams(url, content::Referrer(), disposition,
                               ui::PAGE_TRANSITION_TYPED, false));
    CommitPendingLoad(controller);
  }

  void UserInputAccumulation(const blink::WebInputEvent::Type type) {
    AddTab(browser(), GURL("about:blank"));
    GURL url1("https://www.google.com/");
    GURL url2("http://www.google.com/");
    content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

    scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
    SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(browser()->profile());
    DCHECK(service);

    // Check that navigation triggers engagement.
    NavigateWithDisposition(url1, CURRENT_TAB);
    TrackingStarted(helper.get());

    EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));

    // Simulate a user input trigger and ensure it is treated correctly.
    HandleUserInputAndRestartTracking(helper.get(), type);

    EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));

    // Simulate three inputs , and ensure they are treated correctly.
    HandleUserInputAndRestartTracking(helper.get(), type);
    HandleUserInputAndRestartTracking(helper.get(), type);
    HandleUserInputAndRestartTracking(helper.get(), type);

    EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));

    // Simulate inputs for a different link.
    NavigateWithDisposition(url2, CURRENT_TAB);
    TrackingStarted(helper.get());

    EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
    EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
    EXPECT_DOUBLE_EQ(1.2, service->GetTotalEngagementPoints());

    HandleUserInputAndRestartTracking(helper.get(), type);
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

TEST_F(SiteEngagementHelperTest, MouseWheelEventEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::MouseWheel);
}

TEST_F(SiteEngagementHelperTest, GestureEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::GestureTapDown);
}

TEST_F(SiteEngagementHelperTest, MediaEngagementAccumulation) {
  AddTab(browser(), GURL("about:blank"));
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  NavigateWithDisposition(url1, CURRENT_TAB);
  TrackingStarted(helper.get());

  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate a foreground media input and ensure it is treated correctly.
  HandleMediaPlaying(helper.get(), false);

  EXPECT_DOUBLE_EQ(0.52, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate continual media playing, and ensure it is treated correctly.
  HandleMediaPlaying(helper.get(), false);
  HandleMediaPlaying(helper.get(), false);
  HandleMediaPlaying(helper.get(), false);

  EXPECT_DOUBLE_EQ(0.58, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate backgrounding the media.
  HandleMediaPlaying(helper.get(), true);
  HandleMediaPlaying(helper.get(), true);

  EXPECT_DOUBLE_EQ(0.60, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate inputs for a different link.
  NavigateWithDisposition(url2, CURRENT_TAB);
  TrackingStarted(helper.get());

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.1, service->GetTotalEngagementPoints());

  HandleMediaPlaying(helper.get(), false);
  HandleMediaPlaying(helper.get(), false);
  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.54, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.14, service->GetTotalEngagementPoints());
}

TEST_F(SiteEngagementHelperTest, MediaEngagement) {
  AddTab(browser(), GURL("about:blank"));
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  base::MockTimer* media_tracker_timer = new base::MockTimer(true, false);
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetMediaTrackerPauseTimer(helper.get(), make_scoped_ptr(media_tracker_timer));
  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  NavigateWithDisposition(url1, CURRENT_TAB);
  MediaStartedPlaying(helper.get());

  EXPECT_DOUBLE_EQ(0.50, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.52, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  web_contents->WasHidden();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.53, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  MediaPaused(helper.get());
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.53, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  web_contents->WasShown();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.53, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  MediaStartedPlaying(helper.get());
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  NavigateWithDisposition(url2, CURRENT_TAB);
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.5, service->GetScore(url2));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  MediaStartedPlaying(helper.get());
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.52, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  web_contents->WasHidden();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.53, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  MediaPaused(helper.get());
  web_contents->WasShown();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.53, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());
}

TEST_F(SiteEngagementHelperTest, MixedInputEngagementAccumulation) {
  AddTab(browser(), GURL("about:blank"));
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  base::HistogramTester histograms;

  // Histograms should start off empty.
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              0);

  NavigateWithDisposition(url1, CURRENT_TAB);
  TrackingStarted(helper.get());

  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 1);

  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::RawKeyDown);
  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::GestureTapDown);
  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::GestureTapDown);
  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::RawKeyDown);
  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::MouseDown);

  EXPECT_DOUBLE_EQ(0.75, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              6);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE,
                               2);

  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::MouseWheel);
  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::MouseDown);
  HandleMediaPlaying(helper.get(), true);
  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::GestureTapDown);
  HandleMediaPlaying(helper.get(), false);

  EXPECT_DOUBLE_EQ(0.93, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              11);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_WHEEL, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE,
                               3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_VISIBLE,
                               1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MEDIA_HIDDEN,
                               1);

  NavigateWithDisposition(url2, CURRENT_TAB);
  TrackingStarted(helper.get());

  EXPECT_DOUBLE_EQ(0.93, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.43, service->GetTotalEngagementPoints());

  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::GestureTapDown);
  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::RawKeyDown);

  EXPECT_DOUBLE_EQ(0.93, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.53, service->GetTotalEngagementPoints());
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              14);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE,
                               4);
}

TEST_F(SiteEngagementHelperTest, CheckTimerAndCallbacks) {
  AddTab(browser(), GURL("about:blank"));
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  base::MockTimer* media_tracker_timer = new base::MockTimer(true, false);
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetInputTrackerPauseTimer(helper.get(), make_scoped_ptr(input_tracker_timer));
  SetMediaTrackerPauseTimer(helper.get(), make_scoped_ptr(media_tracker_timer));

  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  NavigateWithDisposition(url1, CURRENT_TAB);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Input timer should be running for navigation delay, but media timer is
  // inactive.
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  // Media timer starts once media is detected as playing.
  MediaStartedPlaying(helper.get());
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  input_tracker_timer->Fire();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.52, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Input timer should start running again after input, but the media timer
  // keeps running.
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper.get()));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  HandleUserInput(helper.get(), blink::WebInputEvent::RawKeyDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.57, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper.get()));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  // Timer should start running again after input.
  HandleUserInput(helper.get(), blink::WebInputEvent::GestureTapDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.62, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper.get()));

  media_tracker_timer->Fire();
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Timer should be running for navigation delay. Media is disabled again.
  NavigateWithDisposition(url2, CURRENT_TAB);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.14, service->GetTotalEngagementPoints());

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper.get()));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  HandleUserInput(helper.get(), blink::WebInputEvent::MouseDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  MediaStartedPlaying(helper.get());
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
  AddTab(browser(), GURL("about:blank"));
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  base::MockTimer* media_tracker_timer = new base::MockTimer(true, false);
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetInputTrackerPauseTimer(helper.get(), make_scoped_ptr(input_tracker_timer));
  SetMediaTrackerPauseTimer(helper.get(), make_scoped_ptr(media_tracker_timer));

  NavigateWithDisposition(url1, CURRENT_TAB);
  input_tracker_timer->Fire();

  // Hiding the tab should stop input tracking. Media tracking remains inactive.
  web_contents->WasHidden();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));

  // Showing the tab should start tracking again after another delay. Media
  // tracking remains inactive.
  web_contents->WasShown();
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));

  // Start media tracking.
  MediaStartedPlaying(helper.get());
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  // Hiding the tab should stop input tracking, but not media tracking.
  web_contents->WasHidden();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));

  // Showing the tab should start tracking again after another delay. Media
  // tracking continues.
  web_contents->WasShown();
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));

  input_tracker_timer->Fire();
  media_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper.get()));
}

// Ensure tracking behavior is correct for multiple navigations in a single tab.
TEST_F(SiteEngagementHelperTest, SingleTabNavigation) {
  AddTab(browser(), GURL("about:blank"));
  GURL url1("https://www.google.com/");
  GURL url2("https://www.example.com/");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetInputTrackerPauseTimer(helper.get(), make_scoped_ptr(input_tracker_timer));

  // Navigation should start the initial delay timer.
  NavigateWithDisposition(url1, CURRENT_TAB);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));

  // Navigating before the timer fires should simply reset the timer.
  NavigateWithDisposition(url2, CURRENT_TAB);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));

  // When the timer fires, callbacks are added.
  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper.get()));

  // Navigation should start the initial delay timer again.
  NavigateWithDisposition(url1, CURRENT_TAB);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper.get()));
}
