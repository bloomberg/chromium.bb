// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ui/blocked_content/popup_tracker.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kTabVisibleTimeAfterRedirect[] =
    "Tab.VisibleTimeAfterCrossOriginRedirect";
constexpr char kTabVisibleTimeAfterRedirectPopup[] =
    "Tab.OpenedPopup.VisibleTimeAfterCrossOriginRedirect";
constexpr char kPopupToRedirect[] =
    "Tab.OpenedPopup.PopupToCrossOriginRedirectTime";

class PopupOpenerTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  PopupOpenerTabHelperTest() : ChromeRenderViewHostTestHarness() {}
  ~PopupOpenerTabHelperTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PopupOpenerTabHelper::CreateForWebContents(web_contents());

    auto tick_clock = base::MakeUnique<base::SimpleTestTickClock>();
    raw_clock_ = tick_clock.get();
    PopupOpenerTabHelper::FromWebContents(web_contents())
        ->set_tick_clock_for_testing(std::move(tick_clock));

    // The tick clock needs to be advanced manually so it isn't set to null,
    // which the code uses to determine if it is set yet.
    raw_clock_->Advance(base::TimeDelta::FromMilliseconds(1));

    EXPECT_TRUE(web_contents()->IsVisible());
  }

  void TearDown() override {
    popups_.clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void NavigateAndCommitWithoutGesture(const GURL& url) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    simulator->SetHasUserGesture(false);
    simulator->Commit();
  }

  // Simulates a popup opened by |web_contents()|.
  content::WebContents* SimulatePopup() {
    std::unique_ptr<content::WebContents> popup(CreateTestWebContents());
    content::WebContents* raw_popup = popup.get();
    popups_.push_back(std::move(popup));

    PopupTracker::CreateForWebContents(raw_popup, web_contents() /* opener */);
    web_contents()->WasHidden();
    raw_popup->WasShown();
    return raw_popup;
  }

  base::SimpleTestTickClock* raw_clock() { return raw_clock_; }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock* raw_clock_ = nullptr;

  std::vector<std::unique_ptr<content::WebContents>> popups_;

  DISALLOW_COPY_AND_ASSIGN(PopupOpenerTabHelperTest);
};

TEST_F(PopupOpenerTabHelperTest, BackgroundNavigation_LogsMetrics) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  web_contents()->WasHidden();
  NavigateAndCommitWithoutGesture(GURL("https://example.test/"));

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  web_contents()->WasShown();
  raw_clock()->Advance(base::TimeDelta::FromSeconds(1));

  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirect, 0);
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirect, 1);
  histogram_tester()->ExpectUniqueSample(
      kTabVisibleTimeAfterRedirect,
      base::TimeDelta::FromSeconds(1).InMilliseconds(), 1);
}

TEST_F(PopupOpenerTabHelperTest, FirstNavigation_NoLogging) {
  web_contents()->WasHidden();
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirect, 0);
}

TEST_F(PopupOpenerTabHelperTest, VisibleNavigation_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  NavigateAndCommitWithoutGesture(GURL("https://example.test/"));
  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirect, 0);
}

TEST_F(PopupOpenerTabHelperTest, AbortedNavigation_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  web_contents()->WasHidden();

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Fail(net::ERR_ABORTED);

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirect, 0);
}

TEST_F(PopupOpenerTabHelperTest, FailedNavigation_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  web_contents()->WasHidden();

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Fail(net::ERR_CONNECTION_RESET);

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirect, 0);
}

TEST_F(PopupOpenerTabHelperTest, SameOriginNavigation_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  web_contents()->WasHidden();
  NavigateAndCommitWithoutGesture(GURL("https://first.test/path"));

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirect, 0);
}

TEST_F(PopupOpenerTabHelperTest, HasUserGesture_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));

  web_contents()->WasHidden();
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetHasUserGesture(true);
  simulator->Commit();

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirect, 0);
}

TEST_F(PopupOpenerTabHelperTest, OpenPopupNoRedirect_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  SimulatePopup();
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirectPopup, 0);
  histogram_tester()->ExpectTotalCount(kPopupToRedirect, 0);
}

TEST_F(PopupOpenerTabHelperTest, SimulateTabUnder_LogsMetrics) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));

  // Popup and then navigate 50ms after.
  SimulatePopup();
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(50));
  NavigateAndCommitWithoutGesture(GURL("https://example.test/"));

  // Spent 100 ms on the opener before closing it.
  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  web_contents()->WasShown();
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(100));
  DeleteContents();

  histogram_tester()->ExpectUniqueSample(kTabVisibleTimeAfterRedirect, 100, 1);
  histogram_tester()->ExpectUniqueSample(kTabVisibleTimeAfterRedirectPopup, 100,
                                         1);
  histogram_tester()->ExpectUniqueSample(kPopupToRedirect, 50, 1);
}

// Same as above tab-under case, but this one starts the navigation before
// issuing the popup. Currently, this case is not supported.
TEST_F(PopupOpenerTabHelperTest, SimulateTabUnderNavBeforePopup_LogsMetrics) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));

  // Start navigating, then popup, then commit.
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(50));
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Start();
  SimulatePopup();
  simulator->Commit();

  // Spent 100 ms on the opener before closing it.
  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  web_contents()->WasShown();
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(100));
  DeleteContents();

  // No histograms are logged because:
  // 1. The navigation starts in the foreground.
  // 2. The popup is issued before the navigation, and the popup metrics only
  //    log for navigations after the popup.
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirect, 0);
  histogram_tester()->ExpectTotalCount(kTabVisibleTimeAfterRedirectPopup, 0);
  histogram_tester()->ExpectTotalCount(kPopupToRedirect, 0);
}
