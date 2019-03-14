// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_PAGE_LOAD_METRICS_OBSERVER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace content {
class NavigationHandle;
}

// Observer responsible for recording metrics for AMP documents. This includes
// both AMP documents loaded in the main frame, and AMP documents loaded in a
// subframe (e.g. in an AMP viewer in the main frame).
//
// For AMP documents loaded in a subframe, recording works like so:
//
// * whenever the main frame URL gets updated with an AMP viewer
//   URL (e.g. https://www.google.com/amp/...), we track that in
//   OnCommitSameDocumentNavigation. we use the time of the main
//   frame URL update as the baseline time from which the
//   user-perceived performance metrics like FCP are computed.
//
// * whenever an iframe AMP document is loaded, we track that in
//   OnDidFinishSubFrameNavigation. we also keep track of the
//   performance timing metrics such as FCP reported in the frame.
//
// * we associate the main frame AMP navigation with its associated
//   AMP document in an iframe by comparing URLs between the main
//   frame and the iframe documents.
//
// * we record AMP metrics at the times when an AMP viewer URL is
//   navigated away from. when a user swipes to change the AMP
//   document, closes a tab, types a new URL in the URL bar, etc,
//   we will record AMP metrics for the AMP doc in an iframe that
//   the user is navigating away from.
class AMPPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // If you add elements to this enum, make sure you update the enum value in
  // enums.xml. Only add elements to the end to prevent inconsistencies between
  // versions.
  enum class AMPViewType {
    NONE,
    AMP_CACHE,
    GOOGLE_SEARCH_AMP_VIEWER,
    GOOGLE_NEWS_AMP_VIEWER,

    // New values should be added before this final entry.
    AMP_VIEW_TYPE_LAST
  };

  static AMPViewType GetAMPViewType(const GURL& url);

  AMPPageLoadMetricsObserver();
  ~AMPPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle,
      const page_load_metrics::PageLoadExtraInfo&) override;
  void OnFrameDeleted(content::RenderFrameHost* rfh) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstLayout(const page_load_metrics::mojom::PageLoadTiming& timing,
                     const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnParseStart(const page_load_metrics::mojom::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnSubFrameRenderDataUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageRenderData& render_data,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;

 private:
  // Information about an AMP navigation in the main frame. Both regular and
  // same document navigations are included.
  struct MainFrameNavigationInfo {
    GURL url;

    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;

    // Pointer to the RenderViewHost for the iframe hosting the AMP document
    // associated with the main frame AMP navigation.
    content::RenderFrameHost* subframe_rfh = nullptr;

    // Navigation start time for the main frame AMP navigation. We use this time
    // as an approximation of the time the user initiated the navigation.
    base::TimeTicks navigation_start;

    // Whether the main frame navigation is a same document navigation.
    bool is_same_document_navigation = false;
  };

  // Information about an AMP subframe.
  struct SubFrameInfo {
    SubFrameInfo();
    ~SubFrameInfo();

    // The AMP viewer URL of the currently committed AMP document. Used for
    // matching the MainFrameNavigationInfo url.
    GURL viewer_url;

    // The navigation start time of the non-same-document navigation in the AMP
    // iframe. Timestamps in |timing| below are relative to this value.
    base::TimeTicks navigation_start;

    // Performance metrics observed in the AMP iframe.
    page_load_metrics::mojom::PageLoadTimingPtr timing;
    page_load_metrics::mojom::PageRenderDataPtr render_data;
  };

  void ProcessMainFrameNavigation(content::NavigationHandle* navigation_handle,
                                  AMPViewType view_type);
  void MaybeRecordAmpDocumentMetrics();

  // Information about the currently active AMP navigation in the main
  // frame. Will be null if there isn't an active AMP navigation in the main
  // frame.
  std::unique_ptr<MainFrameNavigationInfo> current_main_frame_nav_info_;

  // Information about each active AMP iframe in the main document.
  std::map<content::RenderFrameHost*, SubFrameInfo> amp_subframe_info_;

  GURL current_url_;
  AMPViewType view_type_ = AMPViewType::NONE;

  DISALLOW_COPY_AND_ASSIGN(AMPPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_PAGE_LOAD_METRICS_OBSERVER_H_
