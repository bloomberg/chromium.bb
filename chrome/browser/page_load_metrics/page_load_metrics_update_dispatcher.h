// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_UPDATE_DISPATCHER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_UPDATE_DISPATCHER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace page_load_metrics {

class PageLoadMetricsEmbedderInterface;

namespace internal {

// Used to track the status of PageLoadTimings received from the render process.
//
// If you add elements to this enum, make sure you update the enum value in
// histograms.xml. Only add elements to the end to prevent inconsistencies
// between versions.
enum PageLoadTimingStatus {
  // The PageLoadTiming is valid (all data within the PageLoadTiming is
  // consistent with expectations).
  VALID,

  // All remaining status codes are for invalid PageLoadTimings.

  // The PageLoadTiming was empty.
  INVALID_EMPTY_TIMING,

  // The PageLoadTiming had a null navigation_start.
  INVALID_NULL_NAVIGATION_START,

  // Script load or execution durations in the PageLoadTiming were too long.
  INVALID_SCRIPT_LOAD_LONGER_THAN_PARSE,
  INVALID_SCRIPT_EXEC_LONGER_THAN_PARSE,
  INVALID_SCRIPT_LOAD_DOC_WRITE_LONGER_THAN_SCRIPT_LOAD,
  INVALID_SCRIPT_EXEC_DOC_WRITE_LONGER_THAN_SCRIPT_EXEC,

  // The order of two events in the PageLoadTiming was invalid. Either the first
  // wasn't present when the second was present, or the second was reported as
  // happening before the first.
  INVALID_ORDER_RESPONSE_START_PARSE_START,
  INVALID_ORDER_PARSE_START_PARSE_STOP,
  INVALID_ORDER_PARSE_STOP_DOM_CONTENT_LOADED,
  INVALID_ORDER_DOM_CONTENT_LOADED_LOAD,
  INVALID_ORDER_PARSE_START_FIRST_LAYOUT,
  INVALID_ORDER_FIRST_LAYOUT_FIRST_PAINT,
  INVALID_ORDER_FIRST_PAINT_FIRST_TEXT_PAINT,
  INVALID_ORDER_FIRST_PAINT_FIRST_IMAGE_PAINT,
  INVALID_ORDER_FIRST_PAINT_FIRST_CONTENTFUL_PAINT,
  INVALID_ORDER_FIRST_PAINT_FIRST_MEANINGFUL_PAINT,

  // New values should be added before this final entry.
  LAST_PAGE_LOAD_TIMING_STATUS
};

extern const char kPageLoadTimingStatus[];
extern const char kHistogramOutOfOrderTiming[];
extern const char kHistogramOutOfOrderTimingBuffered[];

}  // namespace internal

// PageLoadMetricsUpdateDispatcher manages updates to page load metrics data,
// and dispatches them to the Client. PageLoadMetricsUpdateDispatcher may delay
// dispatching metrics updates to the Client in cases where metrics state hasn't
// stabilized.
class PageLoadMetricsUpdateDispatcher {
 public:
  // The Client class is updated when metrics managed by the dispatcher have
  // changed. Typically it owns the dispatcher.
  class Client {
   public:
    virtual ~Client() {}

    virtual void OnTimingChanged() = 0;
    virtual void OnSubFrameTimingChanged(
        const mojom::PageLoadTiming& timing) = 0;
    virtual void OnMainFrameMetadataChanged() = 0;
    virtual void OnSubframeMetadataChanged() = 0;
    virtual void UpdateFeaturesUsage(
        const mojom::PageLoadFeatures& new_features) = 0;
  };

  // The |client| instance must outlive this object.
  PageLoadMetricsUpdateDispatcher(
      Client* client,
      content::NavigationHandle* navigation_handle,
      PageLoadMetricsEmbedderInterface* embedder_interface);
  ~PageLoadMetricsUpdateDispatcher();

  void UpdateMetrics(content::RenderFrameHost* render_frame_host,
                     const mojom::PageLoadTiming& new_timing,
                     const mojom::PageLoadMetadata& new_metadata,
                     const mojom::PageLoadFeatures& new_features);

  void DidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle);

  void ShutDown();

  const mojom::PageLoadTiming& timing() const {
    return *(current_merged_page_timing_.get());
  }

  const mojom::PageLoadMetadata& main_frame_metadata() const {
    return *(main_frame_metadata_.get());
  }
  const mojom::PageLoadMetadata& subframe_metadata() const {
    return *(subframe_metadata_.get());
  }

 private:
  using FrameTreeNodeId = int;

  void UpdateMainFrameTiming(const mojom::PageLoadTiming& new_timing);
  void UpdateSubFrameTiming(content::RenderFrameHost* render_frame_host,
                            const mojom::PageLoadTiming& new_timing);

  void UpdateMainFrameMetadata(const mojom::PageLoadMetadata& new_metadata);
  void UpdateSubFrameMetadata(const mojom::PageLoadMetadata& subframe_metadata);

  void MaybeDispatchTimingUpdates(bool did_merge_new_timing_value);
  void DispatchTimingUpdates();

  // The client is guaranteed to outlive this object.
  Client* const client_;

  std::unique_ptr<base::Timer> timer_;

  // Time the navigation for this page load was initiated.
  const base::TimeTicks navigation_start_;

  // PageLoadTiming for the currently tracked page. The fields in |paint_timing|
  // are merged across all frames in the document. All other fields are from the
  // main frame document. |current_merged_page_timing_| contains the most recent
  // valid page load timing data, while pending_merged_page_timing_ contains
  // pending updates received since |current_merged_page_timing_| was last
  // dispatched to the client. pending_merged_page_timing_ will be copied to
  // |current_merged_page_timing_| once it is valid, at the time the
  // Client::OnTimingChanged callback is invoked.
  mojom::PageLoadTimingPtr current_merged_page_timing_;
  mojom::PageLoadTimingPtr pending_merged_page_timing_;

  mojom::PageLoadMetadataPtr main_frame_metadata_;
  mojom::PageLoadMetadataPtr subframe_metadata_;

  // Navigation start offsets for the most recently committed document in each
  // frame.
  std::map<FrameTreeNodeId, base::TimeDelta> subframe_navigation_start_offset_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsUpdateDispatcher);
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_UPDATE_DISPATCHER_H_
