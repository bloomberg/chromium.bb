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
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/blocked_content/popup_tracker.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#endif  // defined(OS_ANDROID)

class InfoBarAndroid;

constexpr char kTabUnderVisibleTime[] = "Tab.TabUnder.VisibleTime";
constexpr char kTabUnderVisibleTimeBefore[] = "Tab.TabUnder.VisibleTimeBefore";
constexpr char kPopupToTabUnder[] = "Tab.TabUnder.PopupToTabUnderTime";
constexpr char kTabUnderAction[] = "Tab.TabUnderAction";

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
    InfoBarService::CreateForWebContents(web_contents());

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

TEST_F(PopupOpenerTabHelperTest, LogVisibleTime) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));

  web_contents()->WasHidden();

  NavigateAndCommitWithoutGesture(GURL("https://example.test/"));

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  web_contents()->WasShown();
  raw_clock()->Advance(base::TimeDelta::FromSeconds(1));

  DeleteContents();

  histogram_tester()->ExpectUniqueSample(
      "Tab.VisibleTime", base::TimeDelta::FromSeconds(60 + 1).InMilliseconds(),
      1);
}

TEST_F(PopupOpenerTabHelperTest, SimpleTabUnder_LogsMetrics) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));

  // Spend 1s on the page before doing anything.
  raw_clock()->Advance(base::TimeDelta::FromSeconds(1));

  // Popup and then navigate 50ms after.
  SimulatePopup();
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(50));
  NavigateAndCommitWithoutGesture(GURL("https://example.test/"));

  // Spent 100 ms on the opener before closing it.
  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  web_contents()->WasShown();
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(100));
  DeleteContents();

  histogram_tester()->ExpectUniqueSample(kTabUnderVisibleTimeBefore, 1050, 1);
  histogram_tester()->ExpectUniqueSample(kTabUnderVisibleTime, 100, 1);
  histogram_tester()->ExpectUniqueSample(kPopupToTabUnder, 50, 1);
}

TEST_F(PopupOpenerTabHelperTest, TabUnderAfterRedirect_LogsMetrics) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));

  // Popup and then navigate 50ms after.
  SimulatePopup();
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(50));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://first.test"), main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Start();

  // Foreground the tab before the navigation redirects cross process. Note that
  // currently, this time does not get accounted for in the visibility metrics,
  // though we may want to include it in the future.
  web_contents()->WasShown();
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(10));

  simulator->Redirect(GURL("https://example.test/"));
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(15));

  simulator->Commit();

  // Spent 100 additional ms on the opener before closing it.
  raw_clock()->Advance(base::TimeDelta::FromMilliseconds(100));
  DeleteContents();

  histogram_tester()->ExpectUniqueSample(kTabUnderVisibleTime, 115, 1);
  histogram_tester()->ExpectUniqueSample(kPopupToTabUnder, 60, 1);
}

TEST_F(PopupOpenerTabHelperTest, FirstNavigation_NoLogging) {
  SimulatePopup();
  web_contents()->WasHidden();
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabUnderVisibleTime, 0);
}

TEST_F(PopupOpenerTabHelperTest, VisibleNavigation_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  SimulatePopup();
  web_contents()->WasShown();
  NavigateAndCommitWithoutGesture(GURL("https://example.test/"));
  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabUnderVisibleTime, 0);
}

// This is counter intuitive, but we want to log metrics in the dry-run state if
// we would have blocked the navigation. So, even though the user ends up
// aborting the navigation, it still counts as a tab-under for metrics purposes,
// because we would have intervened.
TEST_F(PopupOpenerTabHelperTest, AbortedNavigationAfterStart_LogsMetrics) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  SimulatePopup();

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Fail(net::ERR_ABORTED);

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabUnderVisibleTime, 1);
}

// See comment in the AbortedNavigation case above.
TEST_F(PopupOpenerTabHelperTest, FailedNavigation_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  SimulatePopup();

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Fail(net::ERR_CONNECTION_RESET);

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabUnderVisibleTime, 1);
}

// Aborts the navigation before the cross-origin redirect.
TEST_F(PopupOpenerTabHelperTest, AbortedNavigationBeforeRedirect_LogsMetrics) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  SimulatePopup();

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://first.test/path"), main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Start();
  simulator->Fail(net::ERR_ABORTED);

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabUnderVisibleTime, 0);
}

TEST_F(PopupOpenerTabHelperTest, SameOriginNavigation_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  SimulatePopup();
  NavigateAndCommitWithoutGesture(GURL("https://first.test/path"));

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabUnderVisibleTime, 0);
}

TEST_F(PopupOpenerTabHelperTest, HasUserGesture_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));

  SimulatePopup();
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetHasUserGesture(true);
  simulator->Commit();

  raw_clock()->Advance(base::TimeDelta::FromMinutes(1));
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabUnderVisibleTime, 0);
}

TEST_F(PopupOpenerTabHelperTest, OpenPopupNoRedirect_NoLogging) {
  NavigateAndCommitWithoutGesture(GURL("https://first.test/"));
  SimulatePopup();
  DeleteContents();
  histogram_tester()->ExpectTotalCount(kTabUnderVisibleTime, 0);
  histogram_tester()->ExpectTotalCount(kPopupToTabUnder, 0);
}

// Same as simple tab-under case, but this one starts the navigation before
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
  histogram_tester()->ExpectTotalCount(kTabUnderVisibleTime, 0);
  histogram_tester()->ExpectTotalCount(kPopupToTabUnder, 0);
}

class BlockTabUnderTest : public PopupOpenerTabHelperTest {
 public:
  BlockTabUnderTest() {}
  ~BlockTabUnderTest() override {}

  void SetUp() override {
    PopupOpenerTabHelperTest::SetUp();
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(
        TabUnderNavigationThrottle::kBlockTabUnders);
  }

  InfoBarAndroid* GetInfoBar() {
#if defined(OS_ANDROID)
    auto* service = InfoBarService::FromWebContents(web_contents());
    if (!service->infobar_count())
      return nullptr;
    EXPECT_EQ(1u, service->infobar_count());
    InfoBarAndroid* infobar =
        static_cast<InfoBarAndroid*>(service->infobar_at(0));
    EXPECT_TRUE(infobar);
    return infobar;
#endif  // defined(OS_ANDROID)
    return nullptr;
  }

  void ExpectUIShown(bool shown) {
#if defined(OS_ANDROID)
    EXPECT_EQ(shown, !!GetInfoBar());
#endif  // defined(OS_ANDROID)
  }

  // content::WebContentsDelegate:
  void OnDidBlockFramebust(content::WebContents* web_contents,
                           const GURL& url) {
    blocked_urls_.push_back(url);
  }

  void DisableFeature() {
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndDisableFeature(
        TabUnderNavigationThrottle::kBlockTabUnders);
  }

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
  ExpectUIShown(true);
}

TEST_F(BlockTabUnderTest, NoFeature_NoBlocking) {
  DisableFeature();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test/")));
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, NoPopup_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  web_contents()->WasHidden();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test/")));
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, SameOriginRedirect_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/path")));
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, BrowserInitiatedNavigation_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://example.test"), web_contents());
  simulator->SetHasUserGesture(false);
  simulator->Commit();
  ExpectUIShown(false);
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
  ExpectUIShown(true);
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
  ExpectUIShown(false);
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
  ExpectUIShown(true);
}

TEST_F(BlockTabUnderTest, TabUnderWithSubsequentGesture_IsNotBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  // First, try navigating without a gesture. It should be blocked.
  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));

  // Now, let the opener get a user gesture. Cast to avoid reaching into private
  // members.
  static_cast<content::WebContentsObserver*>(
      PopupOpenerTabHelper::FromWebContents(web_contents()))
      ->DidGetUserInteraction(blink::WebInputEvent::kMouseDown);

  // A subsequent navigation should be allowed, even if it is classified as a
  // suspicious redirect.
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test2/")));
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, MultipleRedirectAttempts_AreBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  ExpectUIShown(true);
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  ExpectUIShown(true);
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  ExpectUIShown(true);
}

TEST_F(BlockTabUnderTest,
       MultipleRedirectAttempts_AreBlockedAndLogsActionMetrics) {
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
  histogram_tester()->ExpectBucketCount(
      kTabUnderAction,
      static_cast<int>(TabUnderNavigationThrottle::Action::kDidTabUnder), 3);
  histogram_tester()->ExpectTotalCount(kTabUnderAction, 10);
}

// kDidTabUnder is not reported multiple times for redirects.
TEST_F(BlockTabUnderTest, DisableFeature_LogsDidTabUnder) {
  DisableFeature();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  const GURL a_url("https://a.com/");
  const GURL b_url("https://b.com/");
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(a_url, main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Redirect(b_url);
  simulator->Redirect(a_url);
  simulator->Commit();
  histogram_tester()->ExpectBucketCount(
      kTabUnderAction,
      static_cast<int>(TabUnderNavigationThrottle::Action::kStarted), 2);
  histogram_tester()->ExpectBucketCount(
      kTabUnderAction,
      static_cast<int>(TabUnderNavigationThrottle::Action::kDidTabUnder), 1);
  histogram_tester()->ExpectTotalCount(kTabUnderAction, 3);
}
