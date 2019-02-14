// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <bitset>
#include <list>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "net/http/http_response_info.h"
#include "services/metrics/public/cpp/ukm_source.h"

// This observer labels each sub-frame as an ad or not, and keeps track of
// relevant per-frame and whole-page byte statistics.
class AdsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver,
      public subresource_filter::SubresourceFilterObserver {
 public:
  // High level categories of mime types for resources loaded by the page.
  enum class ResourceMimeType {
    kJavascript = 0,
    kVideo = 1,
    kImage = 2,
    kCss = 3,
    kHtml = 4,
    kOther = 5,
    kMaxValue = kOther,
  };

  // Whether or not the adframe size intervention would have triggered on
  // this frame.  These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class AdFrameSizeInterventionStatus {
    kNone = 0,
    kTriggered = 1,
    kMaxValue = kTriggered,
  };

  // Returns a new AdsPageLoadMetricObserver. If the feature is disabled it
  // returns nullptr.
  static std::unique_ptr<AdsPageLoadMetricsObserver> CreateIfNeeded();

  // For a given subframe, returns whether or not the subframe's url would be
  // considering same origin to the main frame's url. |use_parent_origin|
  // indicates that the subframe's parent frames's origin should be used when
  // performing the comparison.
  static bool IsSubframeSameOriginToMainFrame(
      content::RenderFrameHost* sub_host,
      bool use_parent_origin);

  AdsPageLoadMetricsObserver();
  ~AdsPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void RecordAdFrameData(FrameTreeNodeId ad_id,
                         bool is_adframe,
                         content::RenderFrameHost* ad_host,
                         bool frame_navigated);
  void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnResourceDataUseObserved(
      FrameTreeNodeId frame_tree_node_id,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  void OnPageInteractive(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void FrameReceivedFirstUserActivation(content::RenderFrameHost* rfh) override;
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;

 private:
  // subresource_filter::SubresourceFilterObserver:
  void OnSubframeNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      subresource_filter::LoadPolicy load_policy,
      bool is_ad_subframe) override;
  void OnAdSubframeDetected(
      content::RenderFrameHost* render_frame_host) override;
  void OnSubresourceFilterGoingAway() override;

  // Determines if the URL of a frame matches the SubresourceFilter block
  // list. Should only be called once per frame navigation.
  bool DetectSubresourceFilterAd(FrameTreeNodeId frame_tree_node_id);

  // This should only be called once per frame navigation, as the
  // SubresourceFilter detector clears its state about detected frames after
  // each call in order to free up memory.
  bool DetectAds(content::NavigationHandle* navigation_handle);

  void ProcessResourceForFrame(
      FrameTreeNodeId frame_tree_node_id,
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Get the mime type of a resource. This only returns a subset of mime types,
  // grouped at a higher level. For example, all video mime types return the
  // same value.
  ResourceMimeType GetResourceMimeType(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Update all of the per-resource page counters given a new resource data
  // update. Updates |page_resources_| to reflect the new state of the resource.
  // Called once per ResourceDataUpdate.
  void UpdateResource(
      FrameTreeNodeId frame_tree_node_id,
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Records size of resources by mime type.
  void RecordResourceMimeHistograms(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Records per-resource histograms.
  void RecordResourceHistograms(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);
  void RecordPageResourceTotalHistograms(ukm::SourceId source_id);
  void RecordHistograms(ukm::SourceId source_id);
  void RecordHistogramsForAdTagging(FrameData::FrameVisibility visibility);

  // Checks to see if a resource is waiting for a navigation with the given
  // |frame_tree_node_id| to commit before it can be processed. If so, call
  // OnResourceDataUpdate for the delayed resource.
  void ProcessOngoingNavigationResource(FrameTreeNodeId frame_tree_node_id);

  // Stores the size data of each ad frame. Pointed to by ad_frames_ so use a
  // data structure that won't move the data around.
  std::list<FrameData> ad_frames_data_storage_;

  // Maps a frame (by id) to the FrameData responsible for the frame.
  // Multiple frame ids can point to the same FrameData. The responsible
  // frame is the top-most frame labeled as an ad in the frame's ancestry,
  // which may be itself. If no responsible frame is found, the data is
  // nullptr.
  std::map<FrameTreeNodeId, FrameData*> ad_frames_data_;

  // The set of frames that have yet to finish but that the SubresourceFilter
  // has reported are ads. Once DetectSubresourceFilterAd is called the id is
  // removed from the set.
  std::set<FrameTreeNodeId> unfinished_subresource_ad_frames_;

  // When the observer receives report of a document resource loading for a
  // sub-frame before the sub-frame commit occurs, hold onto the resource
  // request info (delay it) until the sub-frame commits.
  std::map<FrameTreeNodeId, page_load_metrics::mojom::ResourceDataUpdatePtr>
      ongoing_navigation_resources_;

  // Maps a request_id for a blink resource to the metadata for the resource
  // load. Only contains resources that have not completed loading.
  std::map<int, page_load_metrics::mojom::ResourceDataUpdatePtr>
      page_resources_;

  // The web contents associated with this page load.
  content::WebContents* web_contents_ = nullptr;

  // Tallies for bytes and counts observed in resource data updates for the
  // entire page.
  size_t page_ad_javascript_bytes_ = 0u;
  size_t page_ad_video_bytes_ = 0u;
  size_t page_ad_resource_bytes_ = 0u;
  size_t page_main_frame_ad_resource_bytes_ = 0u;
  uint32_t total_number_page_resources_ = 0;

  // Flag denoting that this observer should no longer monitor changes in
  // display state for frames. This prevents us from receiving the updates when
  // the frame elements are being destroyed in the renderer.
  bool process_display_state_updates_ = true;

  // Time the page was committed.
  base::Time time_commit_;

  // Time the page was observed to be interactive.
  base::Time time_interactive_;

  // Total ad bytes loaded by the page since it was observed to be interactive.
  size_t page_ad_resource_bytes_since_interactive_ = 0u;

  size_t page_bytes_ = 0u;
  size_t page_network_bytes_ = 0u;
  size_t page_same_origin_bytes_ = 0u;
  bool committed_ = false;

  ScopedObserver<subresource_filter::SubresourceFilterObserverManager,
                 subresource_filter::SubresourceFilterObserver>
      subresource_observer_;

  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
