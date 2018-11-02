// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/stale_while_revalidate_page_load_metrics_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"

namespace internal {
const char kHistogramSWRFirstContentfulPaintNetwork[] =
    "PageLoad.Clients.StaleWhileRevalidate.PaintTiming."
    "FirstContentfulPaint.Network";
const char kHistogramSWRParseToFirstContentfulPaintNetwork[] =
    "PageLoad.Clients.StaleWhileRevalidate.PaintTiming."
    "ParseToFirstContentfulPaint.Network";
const char kHistogramSWRFirstContentfulPaintStaleCache[] =
    "PageLoad.Clients.StaleWhileRevalidate.PaintTiming."
    "FirstContentfulPaint.StaleCache";
const char kHistogramSWRParseToFirstContentfulPaintStaleCache[] =
    "PageLoad.Clients.StaleWhileRevalidate.PaintTiming."
    "ParseToFirstContentfulPaint.StaleCache";
const char kHistogramSWRFirstContentfulPaintCache[] =
    "PageLoad.Clients.StaleWhileRevalidate.PaintTiming."
    "FirstContentfulPaint.Cache";
const char kHistogramSWRParseToFirstContentfulPaintCache[] =
    "PageLoad.Clients.StaleWhileRevalidate.PaintTiming."
    "ParseToFirstContentfulPaint.Cache";

}  // namespace internal

void StaleWhileRevalidatePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    return;
  }

  if ((info.main_frame_metadata.behavior_flags &
       blink::WebLoadingBehaviorFlag::
           kStaleWhileRevalidateResourceCandidateNetworkLoad)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramSWRFirstContentfulPaintNetwork,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSWRParseToFirstContentfulPaintNetwork,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  } else if ((info.main_frame_metadata.behavior_flags &
              blink::WebLoadingBehaviorFlag::
                  kStaleWhileRevalidateResourceCandidateStaleCacheLoad)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramSWRFirstContentfulPaintStaleCache,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSWRParseToFirstContentfulPaintStaleCache,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  } else if ((info.main_frame_metadata.behavior_flags &
              blink::WebLoadingBehaviorFlag::
                  kStaleWhileRevalidateResourceCandidateCacheLoad)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramSWRFirstContentfulPaintCache,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramSWRParseToFirstContentfulPaintCache,
                        timing.paint_timing->first_contentful_paint.value() -
                            timing.parse_timing->parse_start.value());
  }
}
