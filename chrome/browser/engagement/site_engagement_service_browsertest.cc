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
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"

class SiteEngagementServiceBrowserTest : public InProcessBrowserTest {
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

  // Simulate a key press event and handle it.
  void HandleKeyPress(SiteEngagementHelper* helper, ui::KeyboardCode key) {
    content::NativeWebKeyboardEvent event;
    event.windowsKeyCode = key;
    event.type = blink::WebKeyboardEvent::RawKeyDown;
    helper->input_tracker_.HandleKeyPressEvent(event);
  }

  // Simulate a mouse event and handle it.
  void HandleMouseEvent(SiteEngagementHelper* helper,
                        blink::WebMouseEvent::Button button,
                        blink::WebInputEvent::Type type) {
    blink::WebMouseEvent event;
    event.button = button;
    event.type = type;
    helper->input_tracker_.HandleMouseEvent(event);
  }

  // Set a pause timer on the input tracker for test purposes.
  void SetInputTrackerPauseTimer(SiteEngagementHelper* helper,
                                 scoped_ptr<base::Timer> timer) {
    helper->input_tracker_.SetPauseTimerForTesting(timer.Pass());
  }

  bool IsTracking(SiteEngagementHelper* helper) {
    return helper->input_tracker_.is_tracking();
  }

  bool IsActive(SiteEngagementHelper* helper) {
    return helper->input_tracker_.IsActive();
  }

  content::RenderViewHost* InputTrackerHost(SiteEngagementHelper* helper) {
    return helper->input_tracker_.host();
  }
};

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       KeyPressEngagementAccumulation) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  // Check that navigation triggers engagement.
  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate a key press trigger and ensure it is treated correctly.
  HandleKeyPress(helper.get(), ui::VKEY_DOWN);

  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate three key presses, and ensure they are treated correctly.
  HandleKeyPress(helper.get(), ui::VKEY_UP);
  HandleKeyPress(helper.get(), ui::VKEY_RETURN);
  HandleKeyPress(helper.get(), ui::VKEY_J);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate key presses for a different link.
  ui_test_utils::NavigateToURL(browser(), url2);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.2, service->GetTotalEngagementPoints());

  HandleKeyPress(helper.get(), ui::VKEY_K);
  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.25, service->GetTotalEngagementPoints());
}

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       MouseEventEngagementAccumulation) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonLeft,
                   blink::WebInputEvent::MouseDown);

  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonRight,
                   blink::WebInputEvent::MouseWheel);
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonMiddle,
                   blink::WebInputEvent::MouseDown);
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonLeft,
                   blink::WebInputEvent::MouseDown);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  ui_test_utils::NavigateToURL(browser(), url2);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.2, service->GetTotalEngagementPoints());

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonLeft,
                   blink::WebInputEvent::MouseDown);
  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.25, service->GetTotalEngagementPoints());
}

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       MixedInputEngagementAccumulation) {
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

  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 1);

  HandleKeyPress(helper.get(), ui::VKEY_UP);
  HandleKeyPress(helper.get(), ui::VKEY_RETURN);
  HandleKeyPress(helper.get(), ui::VKEY_J);

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonLeft,
                   blink::WebInputEvent::MouseDown);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              5);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 1);

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonRight,
                   blink::WebInputEvent::MouseDown);
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonMiddle,
                   blink::WebInputEvent::MouseDown);
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonNone,
                   blink::WebInputEvent::MouseWheel);

  EXPECT_DOUBLE_EQ(0.85, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              8);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 4);

  ui_test_utils::NavigateToURL(browser(), url2);

  EXPECT_DOUBLE_EQ(0.85, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.35, service->GetTotalEngagementPoints());

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonNone,
                   blink::WebInputEvent::MouseWheel);
  HandleKeyPress(helper.get(), ui::VKEY_DOWN);

  EXPECT_DOUBLE_EQ(0.85, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.45, service->GetTotalEngagementPoints());
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              11);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_NAVIGATION, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_KEYPRESS, 4);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               SiteEngagementMetrics::ENGAGEMENT_MOUSE, 5);
}

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       CheckTimerAndCallbacks) {
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

  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Timer should be running for navigation delay.
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));
  input_tracker_timer->Fire();

  // Timer should start running again after input.
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));
  HandleKeyPress(helper.get(), ui::VKEY_RETURN);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // Timer should start running again after input.
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonNone,
                   blink::WebInputEvent::MouseWheel);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // Timer should be running for navigation delay.
  ui_test_utils::NavigateToURL(browser(), url2);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonRight,
                   blink::WebInputEvent::MouseDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.15, service->GetTotalEngagementPoints());
}

// Ensure that navigation does not trigger input tracking until after a delay.
IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest, ShowAndHide) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetInputTrackerPauseTimer(helper.get(), make_scoped_ptr(input_tracker_timer));

  ui_test_utils::NavigateToURL(browser(), url1);
  input_tracker_timer->Fire();

  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // Hiding the tab should stop input tracking.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  EXPECT_FALSE(IsTracking(helper.get()));

  // Showing the tab should start tracking again after another delay.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // New background tabs should not affect the current tab's input tracking.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // Ensure behavior holds when tab is hidden before the initial delay timer
  // fires.
  ui_test_utils::NavigateToURL(browser(), url2);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  // Showing the tab should start tracking again after another delay.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  input_tracker_timer->Fire();
  EXPECT_TRUE(IsTracking(helper.get()));
}

// Ensure tracking behavior is correct for multiple navigations in a single tab.
IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest, SingleTabNavigation) {
  GURL url1("https://www.google.com/");
  GURL url2("https://www.example.com/");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetInputTrackerPauseTimer(helper.get(), make_scoped_ptr(input_tracker_timer));

  // Navigation should start the initial delay timer.
  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  // Navigating before the timer fires should simply reset the timer.
  ui_test_utils::NavigateToURL(browser(), url2);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  // When the timer fires, callbacks are added.
  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // Navigation should start the initial delay timer again.
  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));
}

// Ensure tracking behavior is correct for multiple navigations in a single tab.
IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest, SwitchRenderViewHost) {
  GURL url1("https://www.google.com/");
  GURL url2("https://www.example.com/");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::MockTimer* input_tracker_timer = new base::MockTimer(true, false);
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetInputTrackerPauseTimer(helper.get(), make_scoped_ptr(input_tracker_timer));

  // Navigation starts the initial delay timer.
  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  content::RenderViewHost* rvh1 = web_contents->GetRenderViewHost();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::RenderViewHost* rvh2 =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetRenderViewHost();

  // The timer should still be running after the RenderViewHost is changed.
  helper->RenderViewHostChanged(rvh1, rvh2);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));
  EXPECT_EQ(rvh2, InputTrackerHost(helper.get()));

  // Firing the timer should add the callbacks.
  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));

  // The callbacks should be on readded another RVH change since the timer has
  // already fired.
  helper->RenderViewHostChanged(rvh2, rvh1);
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));
  EXPECT_EQ(rvh1, InputTrackerHost(helper.get()));

  // Ensure nothing bad happens with a destroyed RVH.
  helper->RenderViewHostChanged(nullptr, rvh2);
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTracking(helper.get()));
  EXPECT_EQ(rvh2, InputTrackerHost(helper.get()));

  // Ensure nothing happens when RVH change happens for a hidden tab.
  helper->WasHidden();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));

  helper->RenderViewHostChanged(rvh2, rvh1);
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTracking(helper.get()));
  EXPECT_EQ(nullptr, InputTrackerHost(helper.get()));
}
