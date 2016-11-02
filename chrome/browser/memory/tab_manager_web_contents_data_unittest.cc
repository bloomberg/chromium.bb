// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_web_contents_data.h"

#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using content::WebContentsTester;

namespace memory {
namespace {

class TabManagerWebContentsDataTest : public ChromeRenderViewHostTestHarness {
 public:
  TabManagerWebContentsDataTest() : ChromeRenderViewHostTestHarness() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_data_ = CreateWebContentsAndTabData(&web_contents_);
    tab_data_->set_test_tick_clock(&test_clock_);
  }

  void TearDown() override {
    tab_data_->set_test_tick_clock(nullptr);
    web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TabManager::WebContentsData* tab_data() { return tab_data_; }

  base::SimpleTestTickClock& test_clock() { return test_clock_; }

  TabManager::WebContentsData* CreateWebContentsAndTabData(
      std::unique_ptr<WebContents>* web_contents) {
    web_contents->reset(
        WebContents::Create(WebContents::CreateParams(profile())));
    TabManager::WebContentsData::CreateForWebContents(web_contents->get());
    return TabManager::WebContentsData::FromWebContents(web_contents->get());
  }

 private:
  std::unique_ptr<WebContents> web_contents_;
  TabManager::WebContentsData* tab_data_;
  base::SimpleTestTickClock test_clock_;
};

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
  EXPECT_EQ(base::TimeTicks::UnixEpoch(), tab_data()->LastAudioChangeTime());
  auto now = base::TimeTicks::Now();
  tab_data()->SetLastAudioChangeTime(now);
  EXPECT_EQ(now, tab_data()->LastAudioChangeTime());
}

TEST_F(TabManagerWebContentsDataTest, LastInactiveTime) {
  EXPECT_EQ(base::TimeTicks::UnixEpoch(), tab_data()->LastInactiveTime());
  auto now = base::TimeTicks::Now();
  tab_data()->SetLastInactiveTime(now);
  EXPECT_EQ(now, tab_data()->LastInactiveTime());
}

TEST_F(TabManagerWebContentsDataTest, CopyState) {
  std::unique_ptr<WebContents> web_contents2;
  auto* tab_data2 = CreateWebContentsAndTabData(&web_contents2);

  EXPECT_EQ(tab_data()->tab_data_, tab_data2->tab_data_);
  tab_data()->IncrementDiscardCount();
  tab_data()->SetDiscardState(true);
  EXPECT_NE(tab_data()->tab_data_, tab_data2->tab_data_);

  TabManager::WebContentsData::CopyState(tab_data()->web_contents(),
                                         tab_data2->web_contents());
  EXPECT_EQ(tab_data()->tab_data_, tab_data2->tab_data_);
  EXPECT_EQ(tab_data()->test_tick_clock_, tab_data2->test_tick_clock_);
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

  tab_data()->SetLastInactiveTime(test_clock().NowTicks());
  test_clock().Advance(base::TimeDelta::FromSeconds(5));
  tab_data()->SetDiscardState(true);
  tab_data()->IncrementDiscardCount();
  test_clock().Advance(base::TimeDelta::FromSeconds(7));
  tab_data()->SetDiscardState(false);

  EXPECT_EQ(1,
            histograms.GetTotalCountsForPrefix(kHistogramName).begin()->second);

  histograms.ExpectBucketCount(kHistogramName, 12000, 1);
}

TEST_F(TabManagerWebContentsDataTest, PurgeAndSuspendState) {
  EXPECT_EQ(TabManager::RUNNING, tab_data()->GetPurgeAndSuspendState());
  base::TimeTicks last_modified = tab_data()->LastPurgeAndSuspendModifiedTime();
  test_clock().Advance(base::TimeDelta::FromSeconds(5));
  tab_data()->SetPurgeAndSuspendState(TabManager::SUSPENDED);
  EXPECT_EQ(TabManager::SUSPENDED, tab_data()->GetPurgeAndSuspendState());
  EXPECT_GT(tab_data()->LastPurgeAndSuspendModifiedTime(), last_modified);

  last_modified = tab_data()->LastPurgeAndSuspendModifiedTime();
  test_clock().Advance(base::TimeDelta::FromSeconds(5));
  tab_data()->SetPurgeAndSuspendState(TabManager::RESUMED);
  EXPECT_EQ(TabManager::RESUMED, tab_data()->GetPurgeAndSuspendState());
  EXPECT_GT(tab_data()->LastPurgeAndSuspendModifiedTime(), last_modified);

  last_modified = tab_data()->LastPurgeAndSuspendModifiedTime();
  test_clock().Advance(base::TimeDelta::FromSeconds(5));
  tab_data()->SetPurgeAndSuspendState(TabManager::RUNNING);
  EXPECT_EQ(TabManager::RUNNING, tab_data()->GetPurgeAndSuspendState());
  EXPECT_GT(tab_data()->LastPurgeAndSuspendModifiedTime(), last_modified);
}

}  // namespace memory
