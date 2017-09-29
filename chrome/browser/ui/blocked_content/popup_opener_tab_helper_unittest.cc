// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kTabVisibleTimeAfterRedirect[] =
    "Tab.VisibleTimeAfterCrossOriginRedirect";

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

    EXPECT_TRUE(web_contents()->IsVisible());
  }

  void NavigateAndCommitWithoutGesture(const GURL& url) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    simulator->SetHasUserGesture(false);
    simulator->Commit();
  }

  base::SimpleTestTickClock* raw_clock() { return raw_clock_; }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock* raw_clock_ = nullptr;

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
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://first.test/"), main_rfh());

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
