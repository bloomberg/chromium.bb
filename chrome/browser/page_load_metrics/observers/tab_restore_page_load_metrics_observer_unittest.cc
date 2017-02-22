// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/tab_restore_page_load_metrics_observer.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace previews {

namespace {

const char kDefaultTestUrl[] = "https://google.com";

class TestTabRestorePageLoadMetricsObserver
    : public TabRestorePageLoadMetricsObserver {
 public:
  explicit TestTabRestorePageLoadMetricsObserver(bool is_restore)
      : is_restore_(is_restore) {}
  ~TestTabRestorePageLoadMetricsObserver() override {}

 private:
  bool IsTabRestore(content::NavigationHandle* navigation_handle) override {
    return is_restore_;
  }

  const bool is_restore_;

  DISALLOW_COPY_AND_ASSIGN(TestTabRestorePageLoadMetricsObserver);
};

}  // namespace

class TabRestorePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  TabRestorePageLoadMetricsObserverTest() {}

  void ResetTest() {
    // Reset to the default testing state. Does not reset histogram state.
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.response_start = base::TimeDelta::FromSeconds(2);
    timing_.parse_start = base::TimeDelta::FromSeconds(3);
    timing_.first_contentful_paint = base::TimeDelta::FromSeconds(4);
    timing_.first_image_paint = base::TimeDelta::FromSeconds(5);
    timing_.first_text_paint = base::TimeDelta::FromSeconds(6);
    timing_.load_event_start = base::TimeDelta::FromSeconds(7);
    PopulateRequiredTimingFields(&timing_);
  }

  void RunTest(content::RestoreType restore_type) {
    is_restore_ = restore_type == content::RestoreType::NONE;
    NavigateAndCommit(GURL(kDefaultTestUrl));
    SimulateTimingUpdate(timing_);

    // Prepare 4 resources of varying size and configurations.
    page_load_metrics::ExtraRequestInfo resources[] = {
        // Cached request.
        {true /*was_cached*/, 1024 * 40 /* raw_body_bytes */,
         false /* data_reduction_proxy_used*/,
         0 /* original_network_content_length */},
        // Uncached non-proxied request.
        {false /*was_cached*/, 1024 * 40 /* raw_body_bytes */,
         false /* data_reduction_proxy_used*/,
         1024 * 40 /* original_network_content_length */},
        // Uncached proxied request with .1 compression ratio.
        {false /*was_cached*/, 1024 * 40 /* raw_body_bytes */,
         false /* data_reduction_proxy_used*/,
         1024 * 40 /* original_network_content_length */},
        // Uncached proxied request with .5 compression ratio.
        {false /*was_cached*/, 1024 * 40 /* raw_body_bytes */,
         false /* data_reduction_proxy_used*/,
         1024 * 40 /* original_network_content_length */},
    };

    int64_t network_bytes = 0;
    int64_t cache_bytes = 0;
    for (auto request : resources) {
      SimulateLoadedResource(request);
      if (!request.was_cached) {
        network_bytes += request.raw_body_bytes;
      } else {
        cache_bytes += request.raw_body_bytes;
      }
    }

    NavigateToUntrackedUrl();
    if (!is_restore_.value()) {
      histogram_tester().ExpectTotalCount(
          "PageLoad.Clients.TabRestore.Experimental.Bytes.Network", 0);
      histogram_tester().ExpectTotalCount(
          "PageLoad.Clients.TabRestore.Experimental.Bytes.Cache", 0);
      histogram_tester().ExpectTotalCount(
          "PageLoad.Clients.TabRestore.Experimental.Bytes.Total", 0);
    } else {
      histogram_tester().ExpectUniqueSample(
          "PageLoad.Clients.TabRestore.Experimental.Bytes.Network",
          static_cast<int>(network_bytes / 1024), 1);
      histogram_tester().ExpectUniqueSample(
          "PageLoad.Clients.TabRestore.Experimental.Bytes.Cache",
          static_cast<int>(cache_bytes / 1024), 1);
      histogram_tester().ExpectUniqueSample(
          "PageLoad.Clients.TabRestore.Experimental.Bytes.Total",
          static_cast<int>((network_bytes + cache_bytes) / 1024), 1);
    }
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(
        new TestTabRestorePageLoadMetricsObserver(is_restore_.value())));
  }

 private:
  base::Optional<bool> is_restore_;
  page_load_metrics::PageLoadTiming timing_;

  DISALLOW_COPY_AND_ASSIGN(TabRestorePageLoadMetricsObserverTest);
};

TEST_F(TabRestorePageLoadMetricsObserverTest, NotRestored) {
  ResetTest();
  RunTest(content::RestoreType::NONE);
}

TEST_F(TabRestorePageLoadMetricsObserverTest, Restored) {
  ResetTest();
  RunTest(content::RestoreType::CURRENT_SESSION);
}

}  // namespace previews
