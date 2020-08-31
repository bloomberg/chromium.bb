// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/core_page_load_metrics_observer.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/common/process_type.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/blink/blink_features.h"

namespace {

// Used to generate a unique id when emitting the "Long Navigation to First
// Contentful Paint" trace event.
int g_num_trace_events_in_process = 0;

// The threshold to emit a trace event is the 99th percentile
// of the histogram on Windows Stable as of Feb 26th, 2020.
constexpr base::TimeDelta kFirstContentfulPaintTraceThreshold =
    base::TimeDelta::FromMilliseconds(12388);

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
                            status,
                            internal::FIRST_MEANINGFUL_PAINT_LAST_ENTRY);
}

std::unique_ptr<base::trace_event::TracedValue> FirstInputDelayTraceData(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  std::unique_ptr<base::trace_event::TracedValue> data =
      std::make_unique<base::trace_event::TracedValue>();
  data->SetDouble(
      "firstInputDelayInMilliseconds",
      timing.interactive_timing->first_input_delay->InMillisecondsF());
  data->SetDouble(
      "navStartToFirstInputTimestampInMilliseconds",
      timing.interactive_timing->first_input_timestamp->InMillisecondsF());
  return data;
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
const char kHistogramFirstPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint";
const char kBackgroundHistogramFirstPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint.Background";
const char kHistogramFirstImagePaint[] =
    "PageLoad.PaintTiming.NavigationToFirstImagePaint";
const char kBackgroundHistogramFirstImagePaint[] =
    "PageLoad.PaintTiming.NavigationToFirstImagePaint.Background";
const char kHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint";
const char kBackgroundHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background";
const char kHistogramFirstContentfulPaintInitiatingProcess[] =
    "PageLoad.Internal.PaintTiming.NavigationToFirstContentfulPaint."
    "InitiatingProcess";
const char kHistogramFirstMeaningfulPaint[] =
    "PageLoad.Experimental.PaintTiming.NavigationToFirstMeaningfulPaint";
const char kHistogramLargestContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint";
const char kHistogramLargestContentfulPaintContentType[] =
    "PageLoad.Internal.PaintTiming.LargestContentfulPaint.ContentType";
const char kHistogramLargestContentfulPaintMainFrame[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint.MainFrame";
const char kHistogramLargestContentfulPaintMainFrameContentType[] =
    "PageLoad.Internal.PaintTiming.LargestContentfulPaint.MainFrame."
    "ContentType";
const char kHistogramFirstInputDelay[] =
    "PageLoad.InteractiveTiming.FirstInputDelay4";
const char kHistogramFirstInputTimestamp[] =
    "PageLoad.InteractiveTiming.FirstInputTimestamp4";
const char kHistogramLongestInputDelay[] =
    "PageLoad.InteractiveTiming.LongestInputDelay4";
const char kHistogramLongestInputTimestamp[] =
    "PageLoad.InteractiveTiming.LongestInputTimestamp4";
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

const char kHistogramFirstContentfulPaintOnBattery[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.OnBattery";
const char kHistogramFirstContentfulPaintNotOnBattery[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NotOnBattery";

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

const char kHistogramFirstContentfulPaintUserInitiated[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.UserInitiated";

const char kHistogramFirstMeaningfulPaintStatus[] =
    "PageLoad.Experimental.PaintTiming.FirstMeaningfulPaintStatus";

const char kHistogramFirstNonScrollInputAfterFirstPaint[] =
    "PageLoad.InputTiming.NavigationToFirstNonScroll.AfterPaint";
const char kHistogramFirstScrollInputAfterFirstPaint[] =
    "PageLoad.InputTiming.NavigationToFirstScroll.AfterPaint";

const char kHistogramPageLoadTotalBytes[] =
    "PageLoad.Experimental.Bytes.Total2";
const char kHistogramPageLoadNetworkBytes[] =
    "PageLoad.Experimental.Bytes.Network";
const char kHistogramPageLoadCacheBytes[] =
    "PageLoad.Experimental.Bytes.Cache2";
const char kHistogramPageLoadNetworkBytesIncludingHeaders[] =
    "PageLoad.Experimental.Bytes.NetworkIncludingHeaders";
const char kHistogramPageLoadUnfinishedBytes[] =
    "PageLoad.Experimental.Bytes.Unfinished";

const char kHistogramPageLoadCpuTotalUsage[] = "PageLoad.Cpu.TotalUsage";
const char kHistogramPageLoadCpuTotalUsageForegrounded[] =
    "PageLoad.Cpu.TotalUsageForegrounded";

const char kHistogramLoadTypeTotalBytesForwardBack[] =
    "PageLoad.Experimental.Bytes.Total2.LoadType.ForwardBackNavigation";
const char kHistogramLoadTypeNetworkBytesForwardBack[] =
    "PageLoad.Experimental.Bytes.Network.LoadType.ForwardBackNavigation";
const char kHistogramLoadTypeCacheBytesForwardBack[] =
    "PageLoad.Experimental.Bytes.Cache2.LoadType.ForwardBackNavigation";

const char kHistogramLoadTypeTotalBytesReload[] =
    "PageLoad.Experimental.Bytes.Total2.LoadType.Reload";
const char kHistogramLoadTypeNetworkBytesReload[] =
    "PageLoad.Experimental.Bytes.Network.LoadType.Reload";
const char kHistogramLoadTypeCacheBytesReload[] =
    "PageLoad.Experimental.Bytes.Cache2.LoadType.Reload";

const char kHistogramLoadTypeTotalBytesNewNavigation[] =
    "PageLoad.Experimental.Bytes.Total2.LoadType.NewNavigation";
const char kHistogramLoadTypeNetworkBytesNewNavigation[] =
    "PageLoad.Experimental.Bytes.Network.LoadType.NewNavigation";
const char kHistogramLoadTypeCacheBytesNewNavigation[] =
    "PageLoad.Experimental.Bytes.Cache2.LoadType.NewNavigation";

const char kHistogramTotalCompletedResources[] =
    "PageLoad.Experimental.CompletedResources.Total2";
const char kHistogramNetworkCompletedResources[] =
    "PageLoad.Experimental.CompletedResources.Network";
const char kHistogramCacheCompletedResources[] =
    "PageLoad.Experimental.CompletedResources.Cache2";

const char kHistogramInputToNavigation[] =
    "PageLoad.Experimental.InputTiming.InputToNavigationStart";
const char kBackgroundHistogramInputToNavigation[] =
    "PageLoad.Experimental.InputTiming.InputToNavigationStart.Background";
const char kHistogramInputToNavigationLinkClick[] =
    "PageLoad.Experimental.InputTiming.InputToNavigationStart.FromLinkClick";
const char kHistogramInputToNavigationOmnibox[] =
    "PageLoad.Experimental.InputTiming.InputToNavigationStart.FromOmnibox";
const char kHistogramInputToFirstPaint[] =
    "PageLoad.Experimental.PaintTiming.InputToFirstPaint";
const char kBackgroundHistogramInputToFirstPaint[] =
    "PageLoad.Experimental.PaintTiming.InputToFirstPaint.Background";
const char kHistogramInputToFirstContentfulPaint[] =
    "PageLoad.Experimental.PaintTiming.InputToFirstContentfulPaint";
const char kBackgroundHistogramInputToFirstContentfulPaint[] =
    "PageLoad.Experimental.PaintTiming.InputToFirstContentfulPaint.Background";

const char kHistogramBackForwardCacheEvent[] =
    "PageLoad.BackForwardCache.Event";

const char kHistogramFontPreloadFirstPaint[] =
    "PageLoad.Clients.FontPreload.PaintTiming.NavigationToFirstPaint";
const char kHistogramFontPreloadFirstContentfulPaint[] =
    "PageLoad.Clients.FontPreload.PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramFontPreloadLargestContentfulPaint[] =
    "PageLoad.Clients.FontPreload.PaintTiming."
    "NavigationToLargestContentfulPaint";

// Navigation metrics from the navigation start.
const char kHistogramNavigationTimingNavigationStartToFirstRequestStart[] =
    "PageLoad.Experimental.NavigationTiming.NavigationStartToFirstRequestStart";
const char kHistogramNavigationTimingNavigationStartToFirstResponseStart[] =
    "PageLoad.Experimental.NavigationTiming."
    "NavigationStartToFirstResponseStart";

// Navigation metrics between milestones.
const char kHistogramNavigationTimingFirstRequestStartToFirstResponseStart[] =
    "PageLoad.Experimental.NavigationTiming."
    "FirstRequestStartToFirstResponseStart";

}  // namespace internal

CorePageLoadMetricsObserver::CorePageLoadMetricsObserver()
    : transition_(ui::PAGE_TRANSITION_LINK),
      was_no_store_main_resource_(false),
      num_cache_resources_(0),
      num_network_resources_(0),
      cache_bytes_(0),
      network_bytes_(0),
      network_bytes_including_headers_(0),
      redirect_chain_size_(0),
      largest_contentful_paint_handler_() {}

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
  RecordNavigationTimingHistograms(navigation_handle);
  return CONTINUE_OBSERVING;
}

void CorePageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
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
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramLoad,
                        timing.document_timing->load_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramLoad,
                        timing.document_timing->load_event_start.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  first_paint_ = GetDelegate().GetNavigationStart() +
                 timing.paint_timing->first_paint.value();
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstPaint,
                        timing.paint_timing->first_paint.value());

    // Note: This depends on PageLoadMetrics internally processing loading
    // behavior before timing metrics if they come in the same IPC update.
    if (font_preload_started_before_rendering_observed_) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFontPreloadFirstPaint,
                          timing.paint_timing->first_paint.value());
    }
    if (timing.input_to_navigation_start) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramInputToFirstPaint,
                          timing.input_to_navigation_start.value() +
                              timing.paint_timing->first_paint.value());
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstPaint,
                        timing.paint_timing->first_paint.value());
    if (timing.input_to_navigation_start) {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramInputToFirstPaint,
                          timing.input_to_navigation_start.value() +
                              timing.paint_timing->first_paint.value());
    }
  }

  if (page_load_metrics::WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramForegroundToFirstPaint,
                        timing.paint_timing->first_paint.value() -
                            GetDelegate().GetFirstForegroundTime().value());
  }
}

void CorePageLoadMetricsObserver::OnFirstImagePaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_image_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstImagePaint,
                        timing.paint_timing->first_image_paint.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstImagePaint,
                        timing.paint_timing->first_image_paint.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseStartToFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value() -
                            timing.parse_timing->parse_start.value());

    // Note: This depends on PageLoadMetrics internally processing loading
    // behavior before timing metrics if they come in the same IPC update.
    if (font_preload_started_before_rendering_observed_) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFontPreloadFirstContentfulPaint,
                          timing.paint_timing->first_contentful_paint.value());
    }

    // Emit a trace event to highlight a long navigation to first contentful
    // paint.
    if (timing.paint_timing->first_contentful_paint >
        kFirstContentfulPaintTraceThreshold) {
      base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
      TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "latency", "Long Navigation to First Contentful Paint",
          TRACE_ID_LOCAL(g_num_trace_events_in_process), navigation_start);
      TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP0(
          "latency", "Long Navigation to First Contentful Paint",
          TRACE_ID_LOCAL(g_num_trace_events_in_process),
          navigation_start +
              timing.paint_timing->first_contentful_paint.value());
      g_num_trace_events_in_process++;
    }

    UMA_HISTOGRAM_ENUMERATION(
        internal::kHistogramFirstContentfulPaintInitiatingProcess,
        GetDelegate().GetUserInitiatedInfo().browser_initiated
            ? content::PROCESS_TYPE_BROWSER
            : content::PROCESS_TYPE_RENDERER,
        content::PROCESS_TYPE_CONTENT_END);

    if (was_no_store_main_resource_) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintNoStore,
                          timing.paint_timing->first_contentful_paint.value());
    }

    if (base::PowerMonitor::IsOnBatteryPower()) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintOnBattery,
                          timing.paint_timing->first_contentful_paint.value());
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintNotOnBattery,
                          timing.paint_timing->first_contentful_paint.value());
    }

    // TODO(bmcquade): consider adding a histogram that uses
    // UserInputInfo.user_input_event.
    if (GetDelegate().GetUserInitiatedInfo().browser_initiated ||
        GetDelegate().GetUserInitiatedInfo().user_gesture) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintUserInitiated,
                          timing.paint_timing->first_contentful_paint.value());
    }

    if (timing.input_to_navigation_start) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramInputToNavigation,
                          timing.input_to_navigation_start.value());
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramInputToFirstContentfulPaint,
          timing.input_to_navigation_start.value() +
              timing.paint_timing->first_contentful_paint.value());

      if (ui::PageTransitionCoreTypeIs(transition_, ui::PAGE_TRANSITION_LINK)) {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramInputToNavigationLinkClick,
                            timing.input_to_navigation_start.value());
      } else if (ui::PageTransitionCoreTypeIs(transition_,
                                              ui::PAGE_TRANSITION_GENERATED) ||
                 ui::PageTransitionCoreTypeIs(transition_,
                                              ui::PAGE_TRANSITION_TYPED)) {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramInputToNavigationOmnibox,
                            timing.input_to_navigation_start.value());
      }
    }

    switch (GetPageLoadType(transition_)) {
      case LOAD_TYPE_RELOAD:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintReload,
            timing.paint_timing->first_contentful_paint.value());
        // TODO(bmcquade): consider adding a histogram that uses
        // UserInputInfo.user_input_event.
        if (GetDelegate().GetUserInitiatedInfo().browser_initiated ||
            GetDelegate().GetUserInitiatedInfo().user_gesture) {
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
    if (timing.input_to_navigation_start) {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramInputToNavigation,
                          timing.input_to_navigation_start.value());
      PAGE_LOAD_HISTOGRAM(
          internal::kBackgroundHistogramInputToFirstContentfulPaint,
          timing.input_to_navigation_start.value() +
              timing.paint_timing->first_contentful_paint.value());
    }
  }

  if (page_load_metrics::WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramForegroundToFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value() -
                            GetDelegate().GetFirstForegroundTime().value());
  }
}

void CorePageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
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

  if (page_load_metrics::WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramForegroundToFirstMeaningfulPaint,
                        timing.paint_timing->first_meaningful_paint.value() -
                            GetDelegate().GetFirstForegroundTime().value());
  }
}

void CorePageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    return;
  }
  UMA_HISTOGRAM_CUSTOM_TIMES(
      internal::kHistogramFirstInputDelay,
      timing.interactive_timing->first_input_delay.value(),
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
      50);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstInputTimestamp,
                      timing.interactive_timing->first_input_timestamp.value());
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "loading", "FirstInputDelay::AllFrames::UMA",
      GetDelegate().GetNavigationStart() +
          timing.interactive_timing->first_input_timestamp.value(),
      "data", FirstInputDelayTraceData(timing));
}

void CorePageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
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
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeDelta parse_duration = timing.parse_timing->parse_stop.value() -
                                   timing.parse_timing->parse_start.value();
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, GetDelegate())) {
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
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordTimingHistograms(timing);
  RecordByteAndResourceHistograms(timing);
  RecordCpuUsageHistograms();
  RecordForegroundDurationHistograms(timing, base::TimeTicks());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CorePageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record final metrics collected up to this point.
  if (GetDelegate().DidCommit()) {
    RecordTimingHistograms(timing);
    RecordByteAndResourceHistograms(timing);
    RecordCpuUsageHistograms();
  }
  RecordForegroundDurationHistograms(timing, base::TimeTicks::Now());
  return STOP_OBSERVING;
}

void CorePageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info) {
  // Only handle actual failures; provisional loads that failed due to another
  // committed load or due to user action are recorded in
  // AbortsPageLoadMetricsObserver.
  if (failed_load_info.error != net::OK &&
      failed_load_info.error != net::ERR_ABORTED) {
    if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
            failed_load_info.time_to_failed_provisional_load, GetDelegate())) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFailedProvisionalLoad,
                          failed_load_info.time_to_failed_provisional_load);
    }
  }
  // Provide an empty PageLoadTiming, since we don't have any timing metrics
  // for failed provisional loads.
  RecordForegroundDurationHistograms(page_load_metrics::mojom::PageLoadTiming(),
                                     base::TimeTicks());
}

void CorePageLoadMetricsObserver::OnUserInput(
    const blink::WebInputEvent& event,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeTicks now;

  if (first_paint_.is_null())
    return;

  // Track clicks after first paint for possible click burst.
  click_tracker_.OnUserInput(event);

  if (!received_non_scroll_input_after_first_paint_) {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureTap ||
        event.GetType() == blink::WebInputEvent::Type::kMouseUp) {
      received_non_scroll_input_after_first_paint_ = true;
      if (now.is_null())
        now = base::TimeTicks::Now();
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFirstNonScrollInputAfterFirstPaint,
          now - first_paint_);
    }
  }
  if (!received_scroll_input_after_first_paint_ &&
      event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    received_scroll_input_after_first_paint_ = true;
    if (now.is_null())
      now = base::TimeTicks::Now();
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstScrollInputAfterFirstPaint,
                        now - first_paint_);
  }
}

void CorePageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    if (resource->is_complete) {
      if (resource->cache_type ==
          page_load_metrics::mojom::CacheType::kNotCached) {
        network_bytes_ += resource->encoded_body_length;
        num_network_resources_++;
      } else {
        cache_bytes_ += resource->encoded_body_length;
        num_cache_resources_++;
      }
    }
    network_bytes_including_headers_ += resource->delta_bytes;
  }
}

void CorePageLoadMetricsObserver::RecordNavigationTimingHistograms(
    content::NavigationHandle* navigation_handle) {
  // Record only main frame navigation.
  DCHECK(navigation_handle->IsInMainFrame());

  // Record metrics for navigation only when all relevant milestones are
  // recorded and in the expected order. It is allowed that they have the same
  // value for some cases (e.g., internal redirection for HSTS).
  if (navigation_handle->NavigationStart().is_null() ||
      navigation_handle->FirstRequestStart().is_null() ||
      navigation_handle->FirstResponseStart().is_null())
    return;
  // TODO(https://crbug.com/1076710): Change these early-returns to DCHECKs
  // after the issue 1076710 is fixed.
  if (navigation_handle->NavigationStart() >
          navigation_handle->FirstRequestStart() ||
      navigation_handle->FirstRequestStart() >
          navigation_handle->FirstResponseStart()) {
    return;
  }

  // Record the elapsed time from the navigation start milestone.
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingNavigationStartToFirstRequestStart,
      navigation_handle->FirstRequestStart() -
          navigation_handle->NavigationStart());
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingNavigationStartToFirstResponseStart,
      navigation_handle->FirstResponseStart() -
          navigation_handle->NavigationStart());

  // Record the intervals between milestones.
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingFirstRequestStartToFirstResponseStart,
      navigation_handle->FirstResponseStart() -
          navigation_handle->FirstRequestStart());
}

// This method records values for metrics that were not recorded during any
// other event, or records failure status for metrics that have not been
// collected yet. This is meant to be called at the end of a page lifetime, for
// example, when the user is navigating away from the page.
void CorePageLoadMetricsObserver::RecordTimingHistograms(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  // Log time to first foreground / time to first background. Log counts that we
  // started a relevant page load in the foreground / background.
  if (!GetDelegate().StartedInForeground() &&
      GetDelegate().GetFirstForegroundTime()) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstForeground,
                        GetDelegate().GetFirstForegroundTime().value());
  }

  const page_load_metrics::ContentfulPaintTimingInfo&
      main_frame_largest_contentful_paint =
          largest_contentful_paint_handler_.MainFrameLargestContentfulPaint();
  if (main_frame_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          main_frame_largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramLargestContentfulPaintMainFrame,
                        main_frame_largest_contentful_paint.Time().value());
    UMA_HISTOGRAM_ENUMERATION(
        internal::kHistogramLargestContentfulPaintMainFrameContentType,
        main_frame_largest_contentful_paint.Type());
  }

  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          largest_contentful_paint_handler_.MergeMainFrameAndSubframes();
  if (all_frames_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramLargestContentfulPaint,
                        all_frames_largest_contentful_paint.Time().value());
    UMA_HISTOGRAM_ENUMERATION(
        internal::kHistogramLargestContentfulPaintContentType,
        all_frames_largest_contentful_paint.Type());
    TRACE_EVENT_MARK_WITH_TIMESTAMP1(
        "loading", "NavStartToLargestContentfulPaint::AllFrames::UMA",
        GetDelegate().GetNavigationStart() +
            all_frames_largest_contentful_paint.Time().value(),
        "data", all_frames_largest_contentful_paint.DataAsTraceValue());

    // Note: This depends on PageLoadMetrics internally processing loading
    // behavior before timing metrics if they come in the same IPC update.
    if (font_preload_started_before_rendering_observed_) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFontPreloadLargestContentfulPaint,
                          all_frames_largest_contentful_paint.Time().value());
    }
  }

  if (main_frame_timing.paint_timing->first_paint &&
      !main_frame_timing.paint_timing->first_meaningful_paint) {
    RecordFirstMeaningfulPaintStatus(
        main_frame_timing.paint_timing->first_contentful_paint
            ? internal::FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_NETWORK_STABLE
            : internal::
                  FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_FIRST_CONTENTFUL_PAINT);
  }

  if (main_frame_timing.interactive_timing->longest_input_timestamp) {
    DCHECK(main_frame_timing.interactive_timing->longest_input_delay);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        internal::kHistogramLongestInputDelay,
        main_frame_timing.interactive_timing->longest_input_delay.value(),
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
        50);
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramLongestInputTimestamp,
        main_frame_timing.interactive_timing->longest_input_timestamp.value());
  }
}

void CorePageLoadMetricsObserver::RecordForegroundDurationHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    base::TimeTicks app_background_time) {
  base::Optional<base::TimeDelta> foreground_duration =
      page_load_metrics::GetInitialForegroundDuration(GetDelegate(),
                                                      app_background_time);
  if (!foreground_duration)
    return;

  if (GetDelegate().DidCommit()) {
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

  if (GetDelegate().GetPageEndReason() == page_load_metrics::END_FORWARD_BACK &&
      GetDelegate().GetUserInitiatedInfo().user_gesture &&
      !GetDelegate().GetUserInitiatedInfo().browser_initiated &&
      GetDelegate().GetPageEndTime() <= foreground_duration) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramUserGestureNavigationToForwardBack,
                        GetDelegate().GetPageEndTime().value());
  }
}

void CorePageLoadMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  total_cpu_usage_ += timing.task_time;

  if (GetDelegate().GetVisibilityTracker().currently_in_foreground()) {
    foreground_cpu_usage_ += timing.task_time;
  }
}

void CorePageLoadMetricsObserver::RecordByteAndResourceHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_GE(network_bytes_, 0);
  DCHECK_GE(cache_bytes_, 0);
  int64_t total_bytes = network_bytes_ + cache_bytes_;

  PAGE_BYTES_HISTOGRAM(internal::kHistogramPageLoadNetworkBytes,
                       network_bytes_);
  PAGE_BYTES_HISTOGRAM(internal::kHistogramPageLoadCacheBytes, cache_bytes_);
  PAGE_BYTES_HISTOGRAM(internal::kHistogramPageLoadTotalBytes, total_bytes);
  PAGE_BYTES_HISTOGRAM(internal::kHistogramPageLoadNetworkBytesIncludingHeaders,
                       network_bytes_including_headers_);

  size_t unfinished_bytes = 0;
  for (auto const& kv :
       GetDelegate().GetResourceTracker().unfinished_resources())
    unfinished_bytes += kv.second->received_data_length;
  PAGE_BYTES_HISTOGRAM(internal::kHistogramPageLoadUnfinishedBytes,
                       unfinished_bytes);

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

  click_tracker_.RecordClickBurst(GetDelegate().GetSourceId());
}

void CorePageLoadMetricsObserver::RecordCpuUsageHistograms() {
  PAGE_LOAD_HISTOGRAM(internal::kHistogramPageLoadCpuTotalUsage,
                      total_cpu_usage_);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramPageLoadCpuTotalUsageForegrounded,
                      foreground_cpu_usage_);
}

void CorePageLoadMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  largest_contentful_paint_handler_.RecordTiming(timing.paint_timing,
                                                 subframe_rfh);
}

void CorePageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  largest_contentful_paint_handler_.OnDidFinishSubFrameNavigation(
      navigation_handle, GetDelegate());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CorePageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  UMA_HISTOGRAM_ENUMERATION(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kEnterBackForwardCache);
  return PageLoadMetricsObserver::OnEnterBackForwardCache(timing);
}

void CorePageLoadMetricsObserver::OnRestoreFromBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // This never reaches yet because OnEnterBackForwardCache returns
  // STOP_OBSERVING.
  // TODO(hajimehoshi): After changing OnEnterBackForwardCache to continue
  // observation, remove the above comment.
  UMA_HISTOGRAM_ENUMERATION(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kRestoreFromBackForwardCache);
}

void CorePageLoadMetricsObserver::OnLoadingBehaviorObserved(
    content::RenderFrameHost* rfh,
    int behavior_flag) {
  if (behavior_flag & blink::LoadingBehaviorFlag::
                          kLoadingBehaviorFontPreloadStartedBeforeRendering) {
    font_preload_started_before_rendering_observed_ = true;
  }
}
