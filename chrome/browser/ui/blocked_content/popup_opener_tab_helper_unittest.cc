// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ui/blocked_content/popup_tracker.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
    auto tick_clock = base::MakeUnique<base::SimpleTestTickClock>();
    raw_clock_ = tick_clock.get();
    PopupOpenerTabHelper::CreateForWebContents(web_contents(),
                                               std::move(tick_clock));

    // The tick clock needs to be advanced manually so it isn't set to null,
    // which the code uses to determine if it is set yet.
    raw_clock_->Advance(base::TimeDelta::FromMilliseconds(1));

    EXPECT_TRUE(web_contents()->IsVisible());
  }

  void TearDown() override {
    popups_.clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Returns the RenderFrameHost the navigation commit in, or nullptr if the
  // navigation failed.
  content::RenderFrameHost* NavigateAndCommitWithoutGesture(const GURL& url) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    simulator->SetHasUserGesture(false);
    simulator->Commit();
    return simulator->GetLastThrottleCheckResult().action() ==
                   content::NavigationThrottle::PROCEED
               ? simulator->GetFinalRenderFrameHost()
               : nullptr;
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
  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));

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

  histogram_tester()->ExpectUniqueSample(
      "Tab.VisibleTime", base::TimeDelta::FromSeconds(60 + 1).InMilliseconds(),
      1);
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

class BlockTabUnderTest : public PopupOpenerTabHelperTest,
                          public content::WebContentsDelegate {
 public:
  BlockTabUnderTest() : content::WebContentsDelegate() {}
  ~BlockTabUnderTest() override {}

  void SetUp() override {
    PopupOpenerTabHelperTest::SetUp();
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(
        TabUnderNavigationThrottle::kBlockTabUnders);
    web_contents()->SetDelegate(this);
  }

  void TearDown() override {
    web_contents()->SetDelegate(nullptr);
    PopupOpenerTabHelperTest::TearDown();
  }

  // content::WebContentsDelegate:
  void OnDidBlockFramebust(content::WebContents* web_contents,
                           const GURL& url) override {
    blocked_urls_.push_back(url);
  }

  void DisableFeature() {
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndDisableFeature(
        TabUnderNavigationThrottle::kBlockTabUnders);
  }

  const std::vector<GURL>& blocked_urls() { return blocked_urls_; }

 private:
  std::vector<GURL> blocked_urls_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BlockTabUnderTest);
};

TEST_F(BlockTabUnderTest, SimpleTabUnder_IsBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  EXPECT_EQ(blocked_url, blocked_urls().back());
}

TEST_F(BlockTabUnderTest, NoFeature_NoBlocking) {
  DisableFeature();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test/")));
  EXPECT_TRUE(blocked_urls().empty());
}

TEST_F(BlockTabUnderTest, NoPopup_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  web_contents()->WasHidden();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test/")));
  EXPECT_TRUE(blocked_urls().empty());
}

TEST_F(BlockTabUnderTest, SameOriginRedirect_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/path")));
  EXPECT_TRUE(blocked_urls().empty());
}

TEST_F(BlockTabUnderTest, BrowserInitiatedNavigation_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://example.test"), web_contents());
  simulator->SetHasUserGesture(false);
  simulator->Commit();
  EXPECT_TRUE(blocked_urls().empty());
}

TEST_F(BlockTabUnderTest, TabUnderCrossOriginRedirect_IsBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  // Navigate to a same-origin URL that redirects cross origin.
  const GURL same_origin("https://first.test/path");
  const GURL blocked_url("https://example.test/");
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(same_origin,
                                                            main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Start();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());
  simulator->Redirect(blocked_url);
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            simulator->GetLastThrottleCheckResult());
  EXPECT_EQ(blocked_url, blocked_urls().back());
}

// Ensure that even though the *redirect* occurred in the background, if the
// navigation started in the foreground there is no blocking.
TEST_F(BlockTabUnderTest,
       TabUnderCrossOriginRedirectFromForeground_IsNotBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  web_contents()->WasShown();

  // Navigate to a same-origin URL that redirects cross origin.
  const GURL same_origin("https://first.test/path");
  const GURL cross_origin("https://example.test/");
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(same_origin,
                                                            main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Start();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());

  web_contents()->WasHidden();

  simulator->Redirect(cross_origin);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());
  simulator->Commit();
  EXPECT_TRUE(blocked_urls().empty());
}

// There is no time limit to this intervention. Explicitly test that navigations
// will be blocked for even days until the tab receives another user gesture.
TEST_F(BlockTabUnderTest, TabUnderWithLongWait_IsBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  // Delay a long time before navigating the opener. Since there is no threshold
  // we always classify as a tab-under.
  raw_clock()->Advance(base::TimeDelta::FromDays(10));

  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  EXPECT_EQ(blocked_url, blocked_urls().back());
}

TEST_F(BlockTabUnderTest, TabUnderWithSubsequentGesture_IsNotBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  // First, try navigating without a gesture. It should be blocked.
  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  EXPECT_EQ(blocked_url, blocked_urls().back());

  // Now, let the opener get a user gesture. Cast to avoid reaching into private
  // members.
  static_cast<content::WebContentsObserver*>(
      PopupOpenerTabHelper::FromWebContents(web_contents()))
      ->DidGetUserInteraction(blink::WebInputEvent::kMouseDown);

  // A subsequent navigation should be allowed, even if it is classified as a
  // suspicious redirect.
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test2/")));
}

TEST_F(BlockTabUnderTest, MultipleRedirectAttempts_AreBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  EXPECT_EQ(blocked_url, blocked_urls().back());
  EXPECT_EQ(3u, blocked_urls().size());
}

TEST_F(BlockTabUnderTest,
       MultipleRedirectAttempts_AreBlockedAndLogsActionMetrics) {
  const char kTabUnderAction[] = "Tab.TabUnderAction";

  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  histogram_tester()->ExpectUniqueSample(
      kTabUnderAction,
      static_cast<int>(TabUnderNavigationThrottle::Action::kStarted), 1);
  SimulatePopup();

  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));

  histogram_tester()->ExpectBucketCount(
      kTabUnderAction,
      static_cast<int>(TabUnderNavigationThrottle::Action::kStarted), 4);
  histogram_tester()->ExpectBucketCount(
      kTabUnderAction,
      static_cast<int>(TabUnderNavigationThrottle::Action::kBlocked), 3);
  histogram_tester()->ExpectTotalCount(kTabUnderAction, 7);
}
