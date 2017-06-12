// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/resource_tracking_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

namespace {
const char kFakeUrl[] = "http://www.google.com/nothingotseehere.html";
}  // namespace

namespace page_load_metrics {

class ResourceTrackingPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  ResourceTrackingPageLoadMetricsObserverTest() : observer_(nullptr) {}

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    std::unique_ptr<page_load_metrics::ResourceTrackingPageLoadMetricsObserver>
        observer(
            new page_load_metrics::ResourceTrackingPageLoadMetricsObserver());
    // Keep track of the observer pointer so we can check it in the unit test.
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

  page_load_metrics::ResourceTrackingPageLoadMetricsObserver* observer() {
    return observer_;
  }

 private:
  // observer_ is owned by the last created PageLoadTracker, and its lifetime is
  // dictated by that tracker's lifetime.
  page_load_metrics::ResourceTrackingPageLoadMetricsObserver* observer_;
};

TEST_F(ResourceTrackingPageLoadMetricsObserverTest, StartAndFinish) {
  page_load_metrics::ExtraRequestStartInfo start_info_1{
      content::ResourceType::RESOURCE_TYPE_IMAGE};

  page_load_metrics::ExtraRequestStartInfo start_info_2{
      content::ResourceType::RESOURCE_TYPE_IMAGE};

  page_load_metrics::ExtraRequestCompleteInfo done_info{
      GURL(),
      net::HostPortPair(),
      -1 /*frame_tree_node_id*/,
      false /*was_cached*/,
      1024 * 40 /* raw_body_bytes */,
      1024 * 40 /* original_network_content_length */,
      nullptr /* data reduction_proxy */,
      content::ResourceType::RESOURCE_TYPE_IMAGE,
      0,
  };

  // Start the navigation. This will create the page load tracker and register
  // the observers.
  NavigateAndCommit(GURL(kFakeUrl));

  // Simulate starting two images, and completing one.
  SimulateStartedResource(start_info_1);
  SimulateStartedResource(start_info_2);
  SimulateLoadedResource(done_info);

  int64_t started = -1;
  int64_t completed = -1;
  EXPECT_NE(nullptr, observer());
  observer()->GetCountsForTypeForTesting(
      content::ResourceType::RESOURCE_TYPE_IMAGE, &started, &completed);
  EXPECT_EQ(2, started);
  EXPECT_EQ(1, completed);
}

}  // namespace page_load_metrics
