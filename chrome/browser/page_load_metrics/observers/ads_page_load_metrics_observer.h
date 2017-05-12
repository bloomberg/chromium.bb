// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <list>
#include <map>
#include <memory>

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "net/http/http_response_info.h"

// This observer labels each sub-frame as an ad or not, and keeps track of
// relevant per-frame and whole-page byte statistics.
class AdsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Returns a new AdsPageLoadMetricObserver. If the feature is disabled it
  // returns nullptr.
  static std::unique_ptr<AdsPageLoadMetricsObserver> CreateIfNeeded();

  AdsPageLoadMetricsObserver();
  ~AdsPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_info) override;
  void OnComplete(const page_load_metrics::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;

 private:
  using FrameTreeNodeId = int;

  struct AdFrameData {
    explicit AdFrameData(FrameTreeNodeId frame_tree_node_id);
    size_t frame_bytes;
    size_t frame_bytes_uncached;
    const FrameTreeNodeId frame_tree_node_id;
  };

  void ProcessLoadedResource(
      const page_load_metrics::ExtraRequestCompleteInfo& extra_request_info);
  void RecordHistograms();

  // Checks to see if a resource is waiting for a navigation with the given
  // |frame_tree_node_id| to commit before it can be processed. If so, call
  // OnLoadedResource for the delayed resource.
  void ProcessOngoingNavigationResource(FrameTreeNodeId frame_tree_node_id);

  // Stores the size data of each ad frame. Pointed to by ad_frames_ so use a
  // data structure that won't move the data around.
  std::list<AdFrameData> ad_frames_data_storage_;

  // Maps a frame (by id) to the AdFrameData responsible for the frame.
  // Multiple frame ids can point to the same AdFrameData. The responsible
  // frame is the top-most frame labeled as an ad in the frame's ancestry,
  // which may be itself. If no responsible frame is found, the data is
  // nullptr.
  std::map<FrameTreeNodeId, AdFrameData*> ad_frames_data_;

  // When the observer receives report of a document resource loading for a
  // sub-frame before the sub-frame commit occurs, hold onto the resource
  // request info (delay it) until the sub-frame commits.
  std::map<FrameTreeNodeId, page_load_metrics::ExtraRequestCompleteInfo>
      ongoing_navigation_resources_;

  size_t page_bytes_ = 0u;
  size_t uncached_page_bytes_ = 0u;
  int top_level_subframe_count_ = 0;
  int top_level_ad_frame_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
