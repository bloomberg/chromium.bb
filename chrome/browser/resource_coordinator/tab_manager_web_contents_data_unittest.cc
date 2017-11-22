// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"

#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebContentsTester;

namespace resource_coordinator {
namespace {

class TabManagerWebContentsDataTest : public ChromeRenderViewHostTestHarness {
 public:
  TabManagerWebContentsDataTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Fast-forward time to prevent the first call to NowTicks() in a test from
    // returning a null TimeTicks.
    test_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_data_ = CreateWebContentsAndTabData(&web_contents_);
  }

  void TearDown() override {
    web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TabManager::WebContentsData* tab_data() { return tab_data_; }

  base::SimpleTestTickClock& test_clock() { return test_clock_; }

  TabManager::WebContentsData* CreateWebContentsAndTabData(
      std::unique_ptr<WebContents>* web_contents) {
    web_contents->reset(
        WebContentsTester::CreateTestWebContents(browser_context(), nullptr));
    TabManager::WebContentsData::CreateForWebContents(web_contents->get());
    return TabManager::WebContentsData::FromWebContents(web_contents->get());
  }

 private:
  std::unique_ptr<WebContents> web_contents_;
  TabManager::WebContentsData* tab_data_;
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
};

const char kDefaultUrl[] = "https://www.google.com";

}  // namespace

TEST_F(TabManagerWebContentsDataTest, DiscardState) {
  EXPECT_FALSE(tab_data()->IsDiscarded());
  tab_data()->SetDiscardState(true);
  EXPECT_TRUE(tab_data()->IsDiscarded());
  tab_data()->SetDiscardState(false);
  EXPECT_FALSE(tab_data()->IsDiscarded());
}

TEST_F(TabManagerWebContentsDataTest, DiscardCount) {
  EXPECT_EQ(0, tab_data()->DiscardCount());
  tab_data()->IncrementDiscardCount();
  EXPECT_EQ(1, tab_data()->DiscardCount());
  tab_data()->IncrementDiscardCount();
  EXPECT_EQ(2, tab_data()->DiscardCount());
}

TEST_F(TabManagerWebContentsDataTest, RecentlyAudible) {
  EXPECT_FALSE(tab_data()->IsRecentlyAudible());
  tab_data()->SetRecentlyAudible(true);
  EXPECT_TRUE(tab_data()->IsRecentlyAudible());
  tab_data()->SetRecentlyAudible(false);
  EXPECT_FALSE(tab_data()->IsRecentlyAudible());
}

TEST_F(TabManagerWebContentsDataTest, LastAudioChangeTime) {
  EXPECT_TRUE(tab_data()->LastAudioChangeTime().is_null());
  auto now = NowTicks();
  tab_data()->SetLastAudioChangeTime(now);
  EXPECT_EQ(now, tab_data()->LastAudioChangeTime());
}

TEST_F(TabManagerWebContentsDataTest, LastInactiveTime) {
  EXPECT_TRUE(tab_data()->LastInactiveTime().is_null());
  auto now = NowTicks();
  tab_data()->SetLastInactiveTime(now);
  EXPECT_EQ(now, tab_data()->LastInactiveTime());
}

TEST_F(TabManagerWebContentsDataTest, TabLoadingState) {
  EXPECT_EQ(TAB_IS_NOT_LOADING, tab_data()->tab_loading_state());
  tab_data()->SetTabLoadingState(TAB_IS_LOADING);
  EXPECT_EQ(TAB_IS_LOADING, tab_data()->tab_loading_state());
  tab_data()->SetTabLoadingState(TAB_IS_LOADED);
  EXPECT_EQ(TAB_IS_LOADED, tab_data()->tab_loading_state());
}

TEST_F(TabManagerWebContentsDataTest, CopyState) {
  std::unique_ptr<WebContents> web_contents2;
  auto* tab_data2 = CreateWebContentsAndTabData(&web_contents2);
  // TabManagerWebContentsData are initially distinct as they each have unique
  // IDs assigned to them at construction time.
  EXPECT_NE(tab_data()->tab_data_, tab_data2->tab_data_);

  // Copying the state should bring the ID along with it, so they should have
  // identical content afterwards.
  TabManager::WebContentsData::CopyState(tab_data()->web_contents(),
                                         tab_data2->web_contents());
  EXPECT_EQ(tab_data()->tab_data_, tab_data2->tab_data_);
}

TEST_F(TabManagerWebContentsDataTest, HistogramDiscardCount) {
  const char kHistogramName[] = "TabManager.Discarding.DiscardCount";

  base::HistogramTester histograms;

  EXPECT_TRUE(histograms.GetTotalCountsForPrefix(kHistogramName).empty());
  tab_data()->SetDiscardState(true);
  EXPECT_EQ(1,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);
  tab_data()->SetDiscardState(false);
  EXPECT_EQ(1,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);
  tab_data()->SetDiscardState(true);
  EXPECT_EQ(2,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);
  tab_data()->SetDiscardState(false);
  EXPECT_EQ(2,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);
}

TEST_F(TabManagerWebContentsDataTest, HistogramReloadCount) {
  const char kHistogramName[] = "TabManager.Discarding.ReloadCount";

  base::HistogramTester histograms;

  EXPECT_TRUE(histograms.GetTotalCountsForPrefix(kHistogramName).empty());
  tab_data()->SetDiscardState(true);
  EXPECT_TRUE(histograms.GetTotalCountsForPrefix(kHistogramName).empty());
  tab_data()->SetDiscardState(false);
  EXPECT_EQ(1,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);
  tab_data()->SetDiscardState(true);
  EXPECT_EQ(1,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);
  tab_data()->SetDiscardState(false);
  EXPECT_EQ(2,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);
}

TEST_F(TabManagerWebContentsDataTest, HistogramsDiscardToReloadTime) {
  const char kHistogramName[] = "TabManager.Discarding.DiscardToReloadTime";

  base::HistogramTester histograms;

  EXPECT_TRUE(histograms.GetTotalCountsForPrefix(kHistogramName).empty());

  tab_data()->SetDiscardState(true);
  test_clock().Advance(base::TimeDelta::FromSeconds(24));
  tab_data()->SetDiscardState(false);

  EXPECT_EQ(1,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);

  histograms.ExpectBucketCount(kHistogramName, 24000, 1);
}

TEST_F(TabManagerWebContentsDataTest, HistogramsReloadToCloseTime) {
  const char kHistogramName[] = "TabManager.Discarding.ReloadToCloseTime";

  base::HistogramTester histograms;

  EXPECT_TRUE(histograms.GetTotalCountsForPrefix(kHistogramName).empty());

  tab_data()->SetDiscardState(true);
  tab_data()->IncrementDiscardCount();
  test_clock().Advance(base::TimeDelta::FromSeconds(5));
  tab_data()->SetDiscardState(false);
  test_clock().Advance(base::TimeDelta::FromSeconds(13));

  tab_data()->WebContentsDestroyed();

  EXPECT_EQ(1,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);

  histograms.ExpectBucketCount(kHistogramName, 13000, 1);
}

TEST_F(TabManagerWebContentsDataTest, HistogramsInactiveToReloadTime) {
  const char kHistogramName[] = "TabManager.Discarding.InactiveToReloadTime";

  base::HistogramTester histograms;

  EXPECT_TRUE(histograms.GetTotalCountsForPrefix(kHistogramName).empty());

  tab_data()->SetLastInactiveTime(NowTicks());
  test_clock().Advance(base::TimeDelta::FromSeconds(5));
  tab_data()->SetDiscardState(true);
  tab_data()->IncrementDiscardCount();
  test_clock().Advance(base::TimeDelta::FromSeconds(7));
  tab_data()->SetDiscardState(false);

  EXPECT_EQ(1,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);

  histograms.ExpectBucketCount(kHistogramName, 12000, 1);
}

TEST_F(TabManagerWebContentsDataTest, IsInSessionRestoreWithTabLoading) {
  EXPECT_FALSE(tab_data()->is_in_session_restore());
  tab_data()->SetIsInSessionRestore(true);
  EXPECT_TRUE(tab_data()->is_in_session_restore());

  WebContents* contents = tab_data()->web_contents();
  WebContentsTester::For(contents)->NavigateAndCommit(GURL(kDefaultUrl));
  WebContentsTester::For(contents)->TestSetIsLoading(false);
  EXPECT_FALSE(tab_data()->is_in_session_restore());
}

TEST_F(TabManagerWebContentsDataTest, IsInSessionRestoreWithTabClose) {
  EXPECT_FALSE(tab_data()->is_in_session_restore());
  tab_data()->SetIsInSessionRestore(true);
  EXPECT_TRUE(tab_data()->is_in_session_restore());

  tab_data()->WebContentsDestroyed();
  EXPECT_FALSE(tab_data()->is_in_session_restore());
}

TEST_F(TabManagerWebContentsDataTest, IsTabRestoredInForeground) {
  EXPECT_FALSE(tab_data()->is_restored_in_foreground());
  tab_data()->SetIsRestoredInForeground(true);
  EXPECT_TRUE(tab_data()->is_restored_in_foreground());
}

}  // namespace resource_coordinator
