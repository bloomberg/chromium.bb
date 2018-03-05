// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "net/http/http_response_headers.h"
#include "ui/base/page_transition_types.h"

namespace {

// The number of buckets in the bitfield histogram. These buckets are described
// in rappor.xml in PageLoad.CoarseTiming.NavigationToFirstContentfulPaint.
// The bucket flag is defined by 1 << bucket_index, and is the bitfield
// representing which timing bucket the page load falls into, i.e. 000010
// would be the bucket flag showing that the page took between 2 and 4 seconds
// to load.
const size_t kNumRapporHistogramBuckets = 6;

uint64_t RapporHistogramBucketIndex(base::TimeDelta time) {
  int64_t seconds = time.InSeconds();
  if (seconds < 2)
    return 0;
  if (seconds < 4)
    return 1;
  if (seconds < 8)
    return 2;
  if (seconds < 16)
    return 3;
  if (seconds < 32)
    return 4;
  return 5;
}

// TODO(bmcquade): If other observers want to log histograms based on load type,
// promote this enum to page_load_metrics_observer.h.
enum PageLoadType {
  LOAD_TYPE_NONE = 0,
  LOAD_TYPE_RELOAD,
  LOAD_TYPE_FORWARD_BACK,
  LOAD_TYPE_NEW_NAVIGATION
};

PageLoadType GetPageLoadType(ui::PageTransition transition) {
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    return LOAD_TYPE_FORWARD_BACK;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) {
    return LOAD_TYPE_RELOAD;
  }
  if (ui::PageTransitionIsNewNavigation(transition)) {
    return LOAD_TYPE_NEW_NAVIGATION;
  }
  NOTREACHED() << "Received PageTransition with no matching PageLoadType.";
  return LOAD_TYPE_NONE;
}

void RecordFirstMeaningfulPaintStatus(
    internal::FirstMeaningfulPaintStatus status) {
  UMA_HISTOGRAM_ENUMERATION(internal::kHistogramFirstMeaningfulPaintStatus,
      status, internal::FIRST_MEANINGFUL_PAINT_LAST_ENTRY);
}

void RecordTimeToInteractiveStatus(internal::TimeToInteractiveStatus status) {
  UMA_HISTOGRAM_ENUMERATION(internal::kHistogramTimeToInteractiveStatus, status,
                            internal::TIME_TO_INTERACTIVE_LAST_ENTRY);
}

}  // namespace

namespace internal {

const char kHistogramDomContentLoaded[] =
    "PageLoad.DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kBackgroundHistogramDomContentLoaded[] =
    "PageLoad.DocumentTiming.NavigationToDOMContentLoadedEventFired.Background";
const char kHistogramLoad[] =
    "PageLoad.DocumentTiming.NavigationToLoadEventFired";
const char kBackgroundHistogramLoad[] =
    "PageLoad.DocumentTiming.NavigationToLoadEventFired.Background";
const char kHistogramFirstLayout[] =
    "PageLoad.DocumentTiming.NavigationToFirstLayout";
const char kBackgroundHistogramFirstLayout[] =
    "PageLoad.DocumentTiming.NavigationToFirstLayout.Background";
const char kHistogramFirstPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint";
const char kBackgroundHistogramFirstPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint.Background";
const char kHistogramFirstTextPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstTextPaint";
const char kBackgroundHistogramFirstTextPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstTextPaint.Background";
const char kHistogramFirstImagePaint[] =
    "PageLoad.PaintTiming.NavigationToFirstImagePaint";
const char kBackgroundHistogramFirstImagePaint[] =
    "PageLoad.PaintTiming.NavigationToFirstImagePaint.Background";
const char kHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint";
const char kBackgroundHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background";
const char kHistogramFirstMeaningfulPaint[] =
    "PageLoad.Experimental.PaintTiming.NavigationToFirstMeaningfulPaint";
const char kHistogramTimeToInteractive[] =
    "PageLoad.Experimental.NavigationToInteractive";
const char kHistogramInteractiveToInteractiveDetection[] =
    "PageLoad.Internal.InteractiveToInteractiveDetection";
const char kHistogramFirstInputDelay[] =
    "PageLoad.InteractiveTiming.FirstInputDelay";
const char kHistogramFirstInputTimestamp[] =
    "PageLoad.InteractiveTiming.FirstInputTimestamp";
const char kHistogramParseStartToFirstMeaningfulPaint[] =
    "PageLoad.Experimental.PaintTiming.ParseStartToFirstMeaningfulPaint";
const char kHistogramParseStartToFirstContentfulPaint[] =
    "PageLoad.PaintTiming.ParseStartToFirstContentfulPaint";
const char kBackgroundHistogramParseStartToFirstContentfulPaint[] =
    "PageLoad.PaintTiming.ParseStartToFirstContentfulPaint.Background";
const char kHistogramParseStart[] =
    "PageLoad.ParseTiming.NavigationToParseStart";
const char kBackgroundHistogramParseStart[] =
    "PageLoad.ParseTiming.NavigationToParseStart.Background";
const char kHistogramParseDuration[] = "PageLoad.ParseTiming.ParseDuration";
const char kBackgroundHistogramParseDuration[] =
    "PageLoad.ParseTiming.ParseDuration.Background";
const char kHistogramParseBlockedOnScriptLoad[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoad";
const char kBackgroundHistogramParseBlockedOnScriptLoad[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoad.Background";
const char kHistogramParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoadFromDocumentWrite";
const char kBackgroundHistogramParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoadFromDocumentWrite."
    "Background";
const char kHistogramParseBlockedOnScriptExecution[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptExecution";
const char kHistogramParseBlockedOnScriptExecutionDocumentWrite[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptExecutionFromDocumentWrite";

const char kHistogramFirstContentfulPaintNoStore[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NoStore";

const char kHistogramLoadTypeFirstContentfulPaintReload[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "Reload";
const char kHistogramLoadTypeFirstContentfulPaintReloadByGesture[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "Reload.UserGesture";
const char kHistogramLoadTypeFirstContentfulPaintForwardBack[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "ForwardBackNavigation";
const char kHistogramLoadTypeFirstContentfulPaintForwardBackNoStore[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "ForwardBackNavigation.NoStore";
const char kHistogramLoadTypeFirstContentfulPaintNewNavigation[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "NewNavigation";

const char kHistogramPageTimingForegroundDuration[] =
    "PageLoad.PageTiming.ForegroundDuration";
const char kHistogramPageTimingForegroundDurationAfterPaint[] =
    "PageLoad.PageTiming.ForegroundDuration.AfterPaint";
const char kHistogramPageTimingForegroundDurationNoCommit[] =
    "PageLoad.PageTiming.ForegroundDuration.NoCommit";
const char kHistogramPageTimingForegroundDurationWithPaint[] =
    "PageLoad.PageTiming.ForegroundDuration.WithPaint";
const char kHistogramPageTimingForegroundDurationWithoutPaint[] =
    "PageLoad.PageTiming.ForegroundDuration.WithoutPaint";

const char kHistogramLoadTypeParseStartReload[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType.Reload";
const char kHistogramLoadTypeParseStartForwardBack[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType."
    "ForwardBackNavigation";
const char kHistogramLoadTypeParseStartForwardBackNoStore[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType."
    "ForwardBackNavigation.NoStore";
const char kHistogramLoadTypeParseStartNewNavigation[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType.NewNavigation";

const char kHistogramFirstForeground[] =
    "PageLoad.PageTiming.NavigationToFirstForeground";

const char kHistogramFailedProvisionalLoad[] =
    "PageLoad.PageTiming.NavigationToFailedProvisionalLoad";

const char kHistogramUserGestureNavigationToForwardBack[] =
    "PageLoad.PageTiming.ForegroundDuration.PageEndReason."
    "ForwardBackNavigation.UserGesture";

const char kHistogramForegroundToFirstPaint[] =
    "PageLoad.PaintTiming.ForegroundToFirstPaint";
const char kHistogramForegroundToFirstContentfulPaint[] =
    "PageLoad.PaintTiming.ForegroundToFirstContentfulPaint";
const char kHistogramForegroundToFirstMeaningfulPaint[] =
    "PageLoad.Experimental.PaintTiming.ForegroundToFirstMeaningfulPaint";

const char kRapporMetricsNameCoarseTiming[] =
    "PageLoad.CoarseTiming.NavigationToFirstContentfulPaint";

const char kRapporMetricsNameFirstMeaningfulPaintNotRecorded[] =
    "PageLoad.Experimental.PaintTiming.FirstMeaningfulPaintNotRecorded";

const char kHistogramFirstContentfulPaintUserInitiated[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.UserInitiated";

const char kHistogramFirstMeaningfulPaintStatus[] =
    "PageLoad.Experimental.PaintTiming.FirstMeaningfulPaintStatus";

const char kHistogramTimeToInteractiveStatus[] =
    "PageLoad.Experimental.TimeToInteractiveStatus";

const char kHistogramFirstNonScrollInputAfterFirstPaint[] =
    "PageLoad.InputTiming.NavigationToFirstNonScroll.AfterPaint";
const char kHistogramFirstScrollInputAfterFirstPaint[] =
    "PageLoad.InputTiming.NavigationToFirstScroll.AfterPaint";

const char kHistogramPageLoadTotalBytes[] = "PageLoad.Experimental.Bytes.Total";
const char kHistogramPageLoadNetworkBytes[] =
    "PageLoad.Experimental.Bytes.Network";
const char kHistogramPageLoadCacheBytes[] = "PageLoad.Experimental.Bytes.Cache";

const char kHistogramLoadTypeTotalBytesForwardBack[] =
    "PageLoad.Experimental.Bytes.Total.LoadType.ForwardBackNavigation";
const char kHistogramLoadTypeNetworkBytesForwardBack[] =
    "PageLoad.Experimental.Bytes.Network.LoadType.ForwardBackNavigation";
const char kHistogramLoadTypeCacheBytesForwardBack[] =
    "PageLoad.Experimental.Bytes.Cache.LoadType.ForwardBackNavigation";

const char kHistogramLoadTypeTotalBytesReload[] =
    "PageLoad.Experimental.Bytes.Total.LoadType.Reload";
const char kHistogramLoadTypeNetworkBytesReload[] =
    "PageLoad.Experimental.Bytes.Network.LoadType.Reload";
const char kHistogramLoadTypeCacheBytesReload[] =
    "PageLoad.Experimental.Bytes.Cache.LoadType.Reload";

const char kHistogramLoadTypeTotalBytesNewNavigation[] =
    "PageLoad.Experimental.Bytes.Total.LoadType.NewNavigation";
const char kHistogramLoadTypeNetworkBytesNewNavigation[] =
    "PageLoad.Experimental.Bytes.Network.LoadType.NewNavigation";
const char kHistogramLoadTypeCacheBytesNewNavigation[] =
    "PageLoad.Experimental.Bytes.Cache.LoadType.NewNavigation";

const char kHistogramTotalCompletedResources[] =
    "PageLoad.Experimental.CompletedResources.Total";
const char kHistogramNetworkCompletedResources[] =
    "PageLoad.Experimental.CompletedResources.Network";
const char kHistogramCacheCompletedResources[] =
    "PageLoad.Experimental.CompletedResources.Cache";

}  // namespace internal

CorePageLoadMetricsObserver::CorePageLoadMetricsObserver()
    : transition_(ui::PAGE_TRANSITION_LINK),
      was_no_store_main_resource_(false),
      num_cache_resources_(0),
      num_network_resources_(0),
      cache_bytes_(0),
      network_bytes_(0),
      redirect_chain_size_(0) {}

CorePageLoadMetricsObserver::~CorePageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CorePageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  redirect_chain_size_++;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CorePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  transition_ = navigation_handle->GetPageTransition();
  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (headers) {
    was_no_store_main_resource_ =
        headers->HasHeaderValue("cache-control", "no-store");
  }
  UMA_HISTOGRAM_COUNTS_100("PageLoad.Navigation.RedirectChainLength",
                           redirect_chain_size_);
  return CONTINUE_OBSERVING;
}

void CorePageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value());
  }
}

void CorePageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramLoad,
                        timing.document_timing->load_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramLoad,
                        timing.document_timing->load_event_start.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstLayout(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->first_layout, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstLayout,
                        timing.document_timing->first_layout.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstLayout,
                        timing.document_timing->first_layout.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  first_paint_ =
      info.navigation_start + timing.paint_timing->first_paint.value();
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstPaint,
                        timing.paint_timing->first_paint.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstPaint,
                        timing.paint_timing->first_paint.value());
  }

  if (WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramForegroundToFirstPaint,
                        timing.paint_timing->first_paint.value() -
                            info.first_foreground_time.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstTextPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_text_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstTextPaint,
                        timing.paint_timing->first_text_paint.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstTextPaint,
                        timing.paint_timing->first_text_paint.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstImagePaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_image_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstImagePaint,
                        timing.paint_timing->first_image_paint.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstImagePaint,
                        timing.paint_timing->first_image_paint.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseStartToFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value() -
                            timing.parse_timing->parse_start.value());

    if (was_no_store_main_resource_) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintNoStore,
                          timing.paint_timing->first_contentful_paint.value());
    }

    // TODO(bmcquade): consider adding a histogram that uses
    // UserInputInfo.user_input_event.
    if (info.user_initiated_info.browser_initiated ||
        info.user_initiated_info.user_gesture) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintUserInitiated,
                          timing.paint_timing->first_contentful_paint.value());
    }

    if (timing.style_sheet_timing
            ->author_style_sheet_parse_duration_before_fcp) {
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.CSSTiming.Parse.BeforeFirstContentfulPaint",
          timing.style_sheet_timing
              ->author_style_sheet_parse_duration_before_fcp.value());
    }
    if (timing.style_sheet_timing->update_style_duration_before_fcp) {
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.CSSTiming.Update.BeforeFirstContentfulPaint",
          timing.style_sheet_timing->update_style_duration_before_fcp.value());
    }
    if (timing.style_sheet_timing
            ->author_style_sheet_parse_duration_before_fcp ||
        timing.style_sheet_timing->update_style_duration_before_fcp) {
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.CSSTiming.ParseAndUpdate.BeforeFirstContentfulPaint",
          timing.style_sheet_timing
                  ->author_style_sheet_parse_duration_before_fcp.value_or(
                      base::TimeDelta()) +
              timing.style_sheet_timing->update_style_duration_before_fcp
                  .value_or(base::TimeDelta()));
    }

    switch (GetPageLoadType(transition_)) {
      case LOAD_TYPE_RELOAD:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintReload,
            timing.paint_timing->first_contentful_paint.value());
        // TODO(bmcquade): consider adding a histogram that uses
        // UserInputInfo.user_input_event.
        if (info.user_initiated_info.browser_initiated ||
            info.user_initiated_info.user_gesture) {
          PAGE_LOAD_HISTOGRAM(
              internal::kHistogramLoadTypeFirstContentfulPaintReloadByGesture,
              timing.paint_timing->first_contentful_paint.value());
        }
        break;
      case LOAD_TYPE_FORWARD_BACK:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintForwardBack,
            timing.paint_timing->first_contentful_paint.value());
        if (was_no_store_main_resource_) {
          PAGE_LOAD_HISTOGRAM(
              internal::
                  kHistogramLoadTypeFirstContentfulPaintForwardBackNoStore,
              timing.paint_timing->first_contentful_paint.value());
        }
        break;
      case LOAD_TYPE_NEW_NAVIGATION:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation,
            timing.paint_timing->first_contentful_paint.value());
        break;
      case LOAD_TYPE_NONE:
        NOTREACHED();
        break;
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramParseStartToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  }

  if (WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramForegroundToFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value() -
                            info.first_foreground_time.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstMeaningfulPaint,
                        timing.paint_timing->first_meaningful_paint.value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseStartToFirstMeaningfulPaint,
                        timing.paint_timing->first_meaningful_paint.value() -
                            timing.parse_timing->parse_start.value());
    RecordFirstMeaningfulPaintStatus(internal::FIRST_MEANINGFUL_PAINT_RECORDED);
  } else {
    RecordFirstMeaningfulPaintStatus(
        internal::FIRST_MEANINGFUL_PAINT_BACKGROUNDED);
  }

  if (WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramForegroundToFirstMeaningfulPaint,
                        timing.paint_timing->first_meaningful_paint.value() -
                            info.first_foreground_time.value());
  }
}

void CorePageLoadMetricsObserver::OnPageInteractive(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // Both interactive and interactive detection time must be present.
  DCHECK(timing.interactive_timing->interactive);
  DCHECK(timing.interactive_timing->interactive_detection);
  DCHECK_GE(timing.interactive_timing->interactive_detection.value(),
            timing.interactive_timing->interactive.value());

  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->interactive_detection, info)) {
    RecordTimeToInteractiveStatus(internal::TIME_TO_INTERACTIVE_BACKGROUNDED);
    return;
  }

  if (timing.interactive_timing->first_invalidating_input &&
      timing.interactive_timing->first_invalidating_input.value() <
          timing.interactive_timing->interactive) {
    RecordTimeToInteractiveStatus(
        internal::TIME_TO_INTERACTIVE_USER_INTERACTION_BEFORE_INTERACTIVE);
    return;
  }

  base::TimeDelta time_to_interactive =
      timing.interactive_timing->interactive.value();
  base::TimeDelta interactive_to_detection =
      timing.interactive_timing->interactive_detection.value() -
      time_to_interactive;
  PAGE_LOAD_HISTOGRAM(internal::kHistogramTimeToInteractive,
                      time_to_interactive);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramInteractiveToInteractiveDetection,
                      interactive_to_detection);
  RecordTimeToInteractiveStatus(internal::TIME_TO_INTERACTIVE_RECORDED);
}

void CorePageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, extra_info)) {
    return;
  }

  // Input delay will often be ~0, and will only be > 10 seconds very
  // rarely. Capture the range from 1ms to 60s.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      internal::kHistogramFirstInputDelay,
      timing.interactive_timing->first_input_delay.value(),
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
      50);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstInputTimestamp,
                      timing.interactive_timing->first_input_timestamp.value());
}

void CorePageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseStart,
                        timing.parse_timing->parse_start.value());

    switch (GetPageLoadType(transition_)) {
      case LOAD_TYPE_RELOAD:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramLoadTypeParseStartReload,
                            timing.parse_timing->parse_start.value());
        break;
      case LOAD_TYPE_FORWARD_BACK:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramLoadTypeParseStartForwardBack,
                            timing.parse_timing->parse_start.value());
        if (was_no_store_main_resource_) {
          PAGE_LOAD_HISTOGRAM(
              internal::kHistogramLoadTypeParseStartForwardBackNoStore,
              timing.parse_timing->parse_start.value());
        }
        break;
      case LOAD_TYPE_NEW_NAVIGATION:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramLoadTypeParseStartNewNavigation,
                            timing.parse_timing->parse_start.value());
        break;
      case LOAD_TYPE_NONE:
        NOTREACHED();
        break;
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramParseStart,
                        timing.parse_timing->parse_start.value());
  }
}

void CorePageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  base::TimeDelta parse_duration = timing.parse_timing->parse_stop.value() -
                                   timing.parse_timing->parse_start.value();
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseDuration, parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_load_from_document_write_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptExecution,
        timing.parse_timing->parse_blocked_on_script_execution_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptExecutionDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_execution_from_document_write_duration
            .value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramParseDuration,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_load_from_document_write_duration
            .value());
  }
}

void CorePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordTimingHistograms(timing, info);
  RecordByteAndResourceHistograms(timing, info);
  RecordRappor(timing, info);
  RecordForegroundDurationHistograms(timing, info, base::TimeTicks());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CorePageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record final metrics collected up to this point.
  if (info.did_commit) {
    RecordTimingHistograms(timing, info);
    RecordByteAndResourceHistograms(timing, info);
  }
  RecordForegroundDurationHistograms(timing, info, base::TimeTicks::Now());
  return STOP_OBSERVING;
}

void CorePageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  // Only handle actual failures; provisional loads that failed due to another
  // committed load or due to user action are recorded in
  // AbortsPageLoadMetricsObserver.
  if (failed_load_info.error != net::OK &&
      failed_load_info.error != net::ERR_ABORTED) {
    if (WasStartedInForegroundOptionalEventInForeground(
            failed_load_info.time_to_failed_provisional_load, extra_info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFailedProvisionalLoad,
                          failed_load_info.time_to_failed_provisional_load);
    }
  }
  // Provide an empty PageLoadTiming, since we don't have any timing metrics
  // for failed provisional loads.
  RecordForegroundDurationHistograms(page_load_metrics::mojom::PageLoadTiming(),
                                     extra_info, base::TimeTicks());
}

void CorePageLoadMetricsObserver::OnUserInput(
    const blink::WebInputEvent& event) {
  base::TimeTicks now;

  if (first_paint_.is_null())
    return;

  if (!received_non_scroll_input_after_first_paint_) {
    if (event.GetType() == blink::WebInputEvent::kGestureTap ||
        event.GetType() == blink::WebInputEvent::kMouseUp) {
      received_non_scroll_input_after_first_paint_ = true;
      if (now.is_null())
        now = base::TimeTicks::Now();
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFirstNonScrollInputAfterFirstPaint,
          now - first_paint_);
    }
  }
  if (!received_scroll_input_after_first_paint_ &&
      event.GetType() == blink::WebInputEvent::kGestureScrollBegin) {
    received_scroll_input_after_first_paint_ = true;
    if (now.is_null())
      now = base::TimeTicks::Now();
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstScrollInputAfterFirstPaint,
                        now - first_paint_);
  }
}

void CorePageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (extra_request_complete_info.was_cached) {
    ++num_cache_resources_;
    cache_bytes_ += extra_request_complete_info.raw_body_bytes;
  } else {
    ++num_network_resources_;
    network_bytes_ += extra_request_complete_info.raw_body_bytes;
  }
}

// This method records values for metrics that were not recorded during any
// other event, or records failure status for metrics that have not been
// collected yet. This is meant to be called at the end of a page lifetime, for
// example, when the user is navigating away from the page.
void CorePageLoadMetricsObserver::RecordTimingHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // Log time to first foreground / time to first background. Log counts that we
  // started a relevant page load in the foreground / background.
  if (!info.started_in_foreground && info.first_foreground_time) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstForeground,
                        info.first_foreground_time.value());
  }

  if (timing.paint_timing->first_paint &&
      !timing.paint_timing->first_meaningful_paint) {
    RecordFirstMeaningfulPaintStatus(
        timing.paint_timing->first_contentful_paint
            ? internal::FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_NETWORK_STABLE
            : internal::
                  FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_FIRST_CONTENTFUL_PAINT);
  }

  if (timing.paint_timing->first_paint &&
      !timing.interactive_timing->interactive) {
    RecordTimeToInteractiveStatus(
        timing.paint_timing->first_meaningful_paint
            ? internal::TIME_TO_INTERACTIVE_DID_NOT_REACH_QUIESCENCE
            : internal::
                  TIME_TO_INTERACTIVE_DID_NOT_REACH_FIRST_MEANINGFUL_PAINT);
  }
}

void CorePageLoadMetricsObserver::RecordForegroundDurationHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info,
    base::TimeTicks app_background_time) {
  base::Optional<base::TimeDelta> foreground_duration =
      GetInitialForegroundDuration(info, app_background_time);
  if (!foreground_duration)
    return;

  if (info.did_commit) {
    PAGE_LOAD_LONG_HISTOGRAM(internal::kHistogramPageTimingForegroundDuration,
                             foreground_duration.value());
    if (timing.paint_timing->first_paint &&
        timing.paint_timing->first_paint < foreground_duration) {
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramPageTimingForegroundDurationAfterPaint,
          foreground_duration.value() -
              timing.paint_timing->first_paint.value());
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramPageTimingForegroundDurationWithPaint,
          foreground_duration.value());
    } else {
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramPageTimingForegroundDurationWithoutPaint,
          foreground_duration.value());
    }
  } else {
    PAGE_LOAD_LONG_HISTOGRAM(
        internal::kHistogramPageTimingForegroundDurationNoCommit,
        foreground_duration.value());
  }

  if (info.page_end_reason == page_load_metrics::END_FORWARD_BACK &&
      info.user_initiated_info.user_gesture &&
      !info.user_initiated_info.browser_initiated &&
      info.page_end_time <= foreground_duration) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramUserGestureNavigationToForwardBack,
                        info.page_end_time.value());
  }
}

void CorePageLoadMetricsObserver::RecordByteAndResourceHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_GE(network_bytes_, 0);
  DCHECK_GE(cache_bytes_, 0);
  int64_t total_bytes = network_bytes_ + cache_bytes_;

  PAGE_BYTES_HISTOGRAM(internal::kHistogramPageLoadNetworkBytes,
                       network_bytes_);
  PAGE_BYTES_HISTOGRAM(internal::kHistogramPageLoadCacheBytes, cache_bytes_);
  PAGE_BYTES_HISTOGRAM(internal::kHistogramPageLoadTotalBytes, total_bytes);

  switch (GetPageLoadType(transition_)) {
    case LOAD_TYPE_RELOAD:
      PAGE_BYTES_HISTOGRAM(internal::kHistogramLoadTypeNetworkBytesReload,
                           network_bytes_);
      PAGE_BYTES_HISTOGRAM(internal::kHistogramLoadTypeCacheBytesReload,
                           cache_bytes_);
      PAGE_BYTES_HISTOGRAM(internal::kHistogramLoadTypeTotalBytesReload,
                           total_bytes);
      break;
    case LOAD_TYPE_FORWARD_BACK:
      PAGE_BYTES_HISTOGRAM(internal::kHistogramLoadTypeNetworkBytesForwardBack,
                           network_bytes_);
      PAGE_BYTES_HISTOGRAM(internal::kHistogramLoadTypeCacheBytesForwardBack,
                           cache_bytes_);
      PAGE_BYTES_HISTOGRAM(internal::kHistogramLoadTypeTotalBytesForwardBack,
                           total_bytes);
      break;
    case LOAD_TYPE_NEW_NAVIGATION:
      PAGE_BYTES_HISTOGRAM(
          internal::kHistogramLoadTypeNetworkBytesNewNavigation,
          network_bytes_);
      PAGE_BYTES_HISTOGRAM(internal::kHistogramLoadTypeCacheBytesNewNavigation,
                           cache_bytes_);
      PAGE_BYTES_HISTOGRAM(internal::kHistogramLoadTypeTotalBytesNewNavigation,
                           total_bytes);
      break;
    case LOAD_TYPE_NONE:
      NOTREACHED();
      break;
  }

  PAGE_RESOURCE_COUNT_HISTOGRAM(internal::kHistogramNetworkCompletedResources,
                                num_network_resources_);
  PAGE_RESOURCE_COUNT_HISTOGRAM(internal::kHistogramCacheCompletedResources,
                                num_cache_resources_);
  PAGE_RESOURCE_COUNT_HISTOGRAM(internal::kHistogramTotalCompletedResources,
                                num_cache_resources_ + num_network_resources_);
}

void CorePageLoadMetricsObserver::RecordRappor(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // During the browser process shutdown path, calling
  // BrowserProcess::rappor_service() can reinitialize multiple destroyed
  // objects. This alters shutdown ordering, so we avoid it by testing
  // IsShuttingDown() first.
  if (g_browser_process->IsShuttingDown())
    return;
  rappor::RapporServiceImpl* rappor_service =
      g_browser_process->rappor_service();
  if (!rappor_service)
    return;
  if (!info.did_commit)
    return;

  // Log the eTLD+1 of sites that show poor loading performance.
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    std::unique_ptr<rappor::Sample> sample =
        rappor_service->CreateSample(rappor::UMA_RAPPOR_TYPE);
    sample->SetStringField(
        "Domain", rappor::GetDomainAndRegistrySampleFromGURL(info.url));
    uint64_t bucket_index = RapporHistogramBucketIndex(
        timing.paint_timing->first_contentful_paint.value());
    sample->SetFlagsField("Bucket", uint64_t(1) << bucket_index,
                          kNumRapporHistogramBuckets);
    // The IsSlow flag is just a one bit boolean if the first contentful paint
    // was > 10s.
    sample->SetFlagsField(
        "IsSlow",
        timing.paint_timing->first_contentful_paint.value().InSecondsF() >= 10,
        1);
    rappor_service->RecordSample(internal::kRapporMetricsNameCoarseTiming,
                                 std::move(sample));
  }

  // Log the eTLD+1 of sites that did not report first meaningful paint.
  if (timing.paint_timing->first_paint &&
      !timing.paint_timing->first_meaningful_paint) {
    rappor::SampleDomainAndRegistryFromGURL(
        rappor_service,
        internal::kRapporMetricsNameFirstMeaningfulPaintNotRecorded, info.url);
  }
}
