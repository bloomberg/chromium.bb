// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/ukm/test_ukm_service.h"
#include "components/ukm/ukm_source.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";

}  // namespace

class UkmPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::MakeUnique<UkmPageLoadMetricsObserver>());
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();

    TestingBrowserProcess::GetGlobal()->SetUkmService(
        ukm_service_test_harness_.test_ukm_service());
  }

  void InitializeTestPageLoadTiming(page_load_metrics::PageLoadTiming* timing) {
    timing->navigation_start = base::Time::FromInternalValue(1);
    timing->first_contentful_paint = base::TimeDelta::FromInternalValue(300);
    PopulateRequiredTimingFields(timing);
  }

  size_t ukm_source_count() {
    return ukm_service_test_harness_.test_ukm_service()->sources_count();
  }

  const ukm::UkmSource* GetUkmSource(size_t source_index) {
    return ukm_service_test_harness_.test_ukm_service()->GetSource(
        source_index);
  }

 private:
  ukm::UkmServiceTestingHarness ukm_service_test_harness_;
};

TEST_F(UkmPageLoadMetricsObserverTest, NoMetrics) {
  EXPECT_EQ(0ul, ukm_source_count());
}

TEST_F(UkmPageLoadMetricsObserverTest, FirstContentfulPaint) {
  page_load_metrics::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());

  const ukm::UkmSource* source = GetUkmSource(0);
  ASSERT_TRUE(source);

  EXPECT_EQ(GURL(kDefaultTestUrl), source->committed_url());
  EXPECT_EQ(base::TimeDelta::FromInternalValue(300),
            source->first_contentful_paint());
}
