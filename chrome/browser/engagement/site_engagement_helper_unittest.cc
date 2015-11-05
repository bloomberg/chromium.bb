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

  void StartTracking(SiteEngagementHelper* helper) {
    helper->input_tracker_.StartTracking();
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
    helper->input_tracker_.StartTracking();
  }

  // Set a pause timer on the input tracker for test purposes.
  void SetInputTrackerPauseTimer(SiteEngagementHelper* helper,
                                 scoped_ptr<base::Timer> timer) {
    helper->input_tracker_.SetPauseTimerForTesting(timer.Pass());
  }

  bool IsTracking(SiteEngagementHelper* helper) {
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
    StartTracking(helper.get());

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
    StartTracking(helper.get());

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
  StartTracking(helper.get());

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
  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::GestureTapDown);

  EXPECT_DOUBLE_EQ(0.9, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              9);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_WHEEL, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE,
                               3);

  NavigateWithDisposition(url2, CURRENT_TAB);
  StartTracking(helper.get());

  EXPECT_DOUBLE_EQ(0.9, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.4, service->GetTotalEngagementPoints());

  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::GestureTapDown);
  HandleUserInputAndRestartTracking(helper.get(),
                                    blink::WebInputEvent::RawKeyDown);

  EXPECT_DOUBLE_EQ(0.9, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.5, service->GetTotalEngagementPoints());
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              12);
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
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetInputTrackerPauseTimer(helper.get(), make_scoped_ptr(input_tracker_timer));

  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  NavigateWithDisposition(url1, CURRENT_TAB);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Timer should be running for navigation delay.
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));
  input_tracker_timer->Fire();

  // Timer should start running again after input.
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));
  HandleUserInput(helper.get(), blink::WebInputEvent::RawKeyDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // Timer should start running again after input.
  HandleUserInput(helper.get(), blink::WebInputEvent::GestureTapDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // Timer should be running for navigation delay.
  NavigateWithDisposition(url2, CURRENT_TAB);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  HandleUserInput(helper.get(), blink::WebInputEvent::MouseDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.15, service->GetTotalEngagementPoints());
}

// Ensure that navigation does not trigger input tracking until after a delay.
// We must manually call WasShown/WasHidden as they are not triggered
// automatically in this test environment.
TEST_F(SiteEngagementHelperTest, ShowAndHide) {
  AddTab(browser(), GURL("about:blank"));
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetInputTrackerPauseTimer(helper.get(), make_scoped_ptr(input_tracker_timer));

  NavigateWithDisposition(url1, CURRENT_TAB);
  input_tracker_timer->Fire();

  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // Hiding the tab should stop input tracking.
  web_contents->WasHidden();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  // Showing the tab should start tracking again after another delay.
  web_contents->WasShown();
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));
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
  EXPECT_FALSE(IsTracking(helper.get()));

  // Navigating before the timer fires should simply reset the timer.
  NavigateWithDisposition(url2, CURRENT_TAB);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  // When the timer fires, callbacks are added.
  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // Navigation should start the initial delay timer again.
  NavigateWithDisposition(url1, CURRENT_TAB);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));
}
