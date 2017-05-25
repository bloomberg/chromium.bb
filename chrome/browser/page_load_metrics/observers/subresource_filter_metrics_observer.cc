// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/subresource_filter_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

using subresource_filter::ContentSubresourceFilterDriverFactory;
using subresource_filter::SubresourceFilterSafeBrowsingActivationThrottle;
using ActivationDecision = subresource_filter::ActivationDecision;

namespace internal {

const char kHistogramSubresourceFilterFirstContentfulPaint[] =
    "PageLoad.Clients.SubresourceFilter.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramSubresourceFilterParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.SubresourceFilter.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramSubresourceFilterFirstMeaningfulPaint[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.PaintTiming."
    "NavigationToFirstMeaningfulPaint";
const char kHistogramSubresourceFilterParseStartToFirstMeaningfulPaint[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.PaintTiming."
    "ParseStartToFirstMeaningfulPaint";

const char kHistogramSubresourceFilterDomContentLoaded[] =
    "PageLoad.Clients.SubresourceFilter.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramSubresourceFilterLoad[] =
    "PageLoad.Clients.SubresourceFilter.DocumentTiming."
    "NavigationToLoadEventFired";

const char kHistogramSubresourceFilterParseDuration[] =
    "PageLoad.Clients.SubresourceFilter.ParseTiming.ParseDuration";
const char kHistogramSubresourceFilterParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.SubresourceFilter.ParseTiming.ParseBlockedOnScriptLoad";
const char kHistogramSubresourceFilterParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.SubresourceFilter.ParseTiming."
    "ParseBlockedOnScriptLoadFromDocumentWrite";
const char kHistogramSubresourceFilterParseBlockedOnScriptExecution[] =
    "PageLoad.Clients.SubresourceFilter.ParseTiming."
    "ParseBlockedOnScriptExecution";
const char
    kHistogramSubresourceFilterParseBlockedOnScriptExecutionDocumentWrite[] =
        "PageLoad.Clients.SubresourceFilter.ParseTiming."
        "ParseBlockedOnScriptExecutionFromDocumentWrite";

const char kHistogramSubresourceFilterTotalResources[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.CompletedResources.Total";
const char kHistogramSubresourceFilterNetworkResources[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.CompletedResources."
    "Network";
const char kHistogramSubresourceFilterCacheResources[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.CompletedResources.Cache";

const char kHistogramSubresourceFilterTotalBytes[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.Bytes.Total";
const char kHistogramSubresourceFilterNetworkBytes[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.Bytes.Network";
const char kHistogramSubresourceFilterCacheBytes[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.Bytes.Cache";

const char kHistogramSubresourceFilterMediaTotalResources[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.CompletedResources.Total."
    "MediaPlayed";
const char kHistogramSubresourceFilterMediaNetworkResources[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.CompletedResources."
    "Network.MediaPlayed";
const char kHistogramSubresourceFilterMediaCacheResources[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.CompletedResources.Cache."
    "MediaPlayed";
const char kHistogramSubresourceFilterMediaTotalBytes[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.Bytes.Total.MediaPlayed";
const char kHistogramSubresourceFilterMediaNetworkBytes[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.Bytes.Network.MediaPlayed";
const char kHistogramSubresourceFilterMediaCacheBytes[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.Bytes.Cache.MediaPlayed";

const char kHistogramSubresourceFilterNoMediaTotalResources[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.CompletedResources.Total."
    "NoMediaPlayed";
const char kHistogramSubresourceFilterNoMediaNetworkResources[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.CompletedResources."
    "Network.NoMediaPlayed";
const char kHistogramSubresourceFilterNoMediaCacheResources[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.CompletedResources.Cache."
    "NoMediaPlayed";
const char kHistogramSubresourceFilterNoMediaTotalBytes[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.Bytes.Total.NoMediaPlayed";
const char kHistogramSubresourceFilterNoMediaNetworkBytes[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.Bytes.Network."
    "NoMediaPlayed";
const char kHistogramSubresourceFilterNoMediaCacheBytes[] =
    "PageLoad.Clients.SubresourceFilter.Experimental.Bytes.Cache.NoMediaPlayed";

const char kHistogramSubresourceFilterForegroundDuration[] =
    "PageLoad.Clients.SubresourceFilter.PageTiming.ForegroundDuration";
const char kHistogramSubresourceFilterForegroundDurationAfterPaint[] =
    "PageLoad.Clients.SubresourceFilter.PageTiming.ForegroundDuration."
    "AfterPaint";

const char kHistogramSubresourceFilterCount[] =
    "PageLoad.Clients.SubresourceFilter.Count";

const char kHistogramSubresourceFilterActivationDecision[] =
    "PageLoad.Clients.SubresourceFilter.ActivationDecision";
const char kHistogramSubresourceFilterActivationDecisionReload[] =
    "PageLoad.Clients.SubresourceFilter.ActivationDecision.LoadType.Reload";

}  // namespace internal

namespace {

void LogActivationDecisionMetrics(content::NavigationHandle* navigation_handle,
                                  ActivationDecision decision) {
  UMA_HISTOGRAM_ENUMERATION(
      internal::kHistogramSubresourceFilterActivationDecision,
      static_cast<int>(decision),
      static_cast<int>(ActivationDecision::ACTIVATION_DECISION_MAX));

  if (SubresourceFilterSafeBrowsingActivationThrottle::NavigationIsPageReload(
          navigation_handle)) {
    UMA_HISTOGRAM_ENUMERATION(
        internal::kHistogramSubresourceFilterActivationDecisionReload,
        static_cast<int>(decision),
        static_cast<int>(ActivationDecision::ACTIVATION_DECISION_MAX));
  }
}

}  // namespace

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceFilterMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record final metrics collected up to this point.
  if (info.did_commit)
    OnGoingAway(timing, info, base::TimeTicks::Now());
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceFilterMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  const auto* subres_filter =
      ContentSubresourceFilterDriverFactory::FromWebContents(
          navigation_handle->GetWebContents());
  if (subres_filter)
    LogActivationDecisionMetrics(
        navigation_handle,
        subres_filter->GetActivationDecisionForLastCommittedPageLoad());
  return CONTINUE_OBSERVING;
}

void SubresourceFilterMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  OnGoingAway(timing, info, base::TimeTicks());
}

void SubresourceFilterMetricsObserver::OnLoadedResource(
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

void SubresourceFilterMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!subresource_filter_observed_)
    return;

  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, info))
    return;

  base::TimeDelta parse_duration = timing.parse_timing->parse_stop.value() -
                                   timing.parse_timing->parse_start.value();
  PAGE_LOAD_HISTOGRAM(internal::kHistogramSubresourceFilterParseDuration,
                      parse_duration);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptLoad,
      timing.parse_timing->parse_blocked_on_script_load_duration.value());
  PAGE_LOAD_HISTOGRAM(
      internal::
          kHistogramSubresourceFilterParseBlockedOnScriptLoadDocumentWrite,
      timing.parse_timing
          ->parse_blocked_on_script_load_from_document_write_duration.value());
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptExecution,
      timing.parse_timing->parse_blocked_on_script_execution_duration.value());
  PAGE_LOAD_HISTOGRAM(
      internal::
          kHistogramSubresourceFilterParseBlockedOnScriptExecutionDocumentWrite,
      timing.parse_timing
          ->parse_blocked_on_script_execution_from_document_write_duration
          .value());
}

void SubresourceFilterMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!subresource_filter_observed_)
    return;

  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSubresourceFilterFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSubresourceFilterParseStartToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
}

void SubresourceFilterMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (!subresource_filter_observed_)
    return;

  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSubresourceFilterFirstMeaningfulPaint,
        timing.paint_timing->first_meaningful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSubresourceFilterParseStartToFirstMeaningfulPaint,
        timing.paint_timing->first_meaningful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
}

void SubresourceFilterMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!subresource_filter_observed_)
    return;

  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSubresourceFilterDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value());
  }
}

void SubresourceFilterMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!subresource_filter_observed_)
    return;

  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramSubresourceFilterLoad,
                        timing.document_timing->load_event_start.value());
  }
}

void SubresourceFilterMetricsObserver::OnLoadingBehaviorObserved(
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (subresource_filter_observed_)
    return;

  subresource_filter_observed_ =
      page_load_metrics::DidObserveLoadingBehaviorInAnyFrame(
          info, blink::WebLoadingBehaviorFlag::
                    kWebLoadingBehaviorSubresourceFilterMatch);

  if (subresource_filter_observed_) {
    UMA_HISTOGRAM_BOOLEAN(internal::kHistogramSubresourceFilterCount, true);
  }
}

void SubresourceFilterMetricsObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    bool is_in_main_frame) {
  played_media_ = true;
}

void SubresourceFilterMetricsObserver::OnGoingAway(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info,
    base::TimeTicks app_background_time) {
  if (!subresource_filter_observed_)
    return;

  PAGE_RESOURCE_COUNT_HISTOGRAM(
      internal::kHistogramSubresourceFilterNetworkResources,
      num_network_resources_);
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      internal::kHistogramSubresourceFilterCacheResources,
      num_cache_resources_);
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      internal::kHistogramSubresourceFilterTotalResources,
      num_cache_resources_ + num_network_resources_);

  PAGE_BYTES_HISTOGRAM(internal::kHistogramSubresourceFilterNetworkBytes,
                       network_bytes_);
  PAGE_BYTES_HISTOGRAM(internal::kHistogramSubresourceFilterCacheBytes,
                       cache_bytes_);
  PAGE_BYTES_HISTOGRAM(internal::kHistogramSubresourceFilterTotalBytes,
                       cache_bytes_ + network_bytes_);

  if (played_media_) {
    PAGE_RESOURCE_COUNT_HISTOGRAM(
        internal::kHistogramSubresourceFilterMediaNetworkResources,
        num_network_resources_);
    PAGE_RESOURCE_COUNT_HISTOGRAM(
        internal::kHistogramSubresourceFilterMediaCacheResources,
        num_cache_resources_);
    PAGE_RESOURCE_COUNT_HISTOGRAM(
        internal::kHistogramSubresourceFilterMediaTotalResources,
        num_cache_resources_ + num_network_resources_);

    PAGE_BYTES_HISTOGRAM(internal::kHistogramSubresourceFilterMediaNetworkBytes,
                         network_bytes_);
    PAGE_BYTES_HISTOGRAM(internal::kHistogramSubresourceFilterMediaCacheBytes,
                         cache_bytes_);
    PAGE_BYTES_HISTOGRAM(internal::kHistogramSubresourceFilterMediaTotalBytes,
                         cache_bytes_ + network_bytes_);
  } else {
    PAGE_RESOURCE_COUNT_HISTOGRAM(
        internal::kHistogramSubresourceFilterNoMediaNetworkResources,
        num_network_resources_);
    PAGE_RESOURCE_COUNT_HISTOGRAM(
        internal::kHistogramSubresourceFilterNoMediaCacheResources,
        num_cache_resources_);
    PAGE_RESOURCE_COUNT_HISTOGRAM(
        internal::kHistogramSubresourceFilterNoMediaTotalResources,
        num_cache_resources_ + num_network_resources_);

    PAGE_BYTES_HISTOGRAM(
        internal::kHistogramSubresourceFilterNoMediaNetworkBytes,
        network_bytes_);
    PAGE_BYTES_HISTOGRAM(internal::kHistogramSubresourceFilterNoMediaCacheBytes,
                         cache_bytes_);
    PAGE_BYTES_HISTOGRAM(internal::kHistogramSubresourceFilterNoMediaTotalBytes,
                         cache_bytes_ + network_bytes_);
  }

  base::Optional<base::TimeDelta> foreground_duration =
      GetInitialForegroundDuration(info, app_background_time);
  if (foreground_duration) {
    PAGE_LOAD_LONG_HISTOGRAM(
        internal::kHistogramSubresourceFilterForegroundDuration,
        foreground_duration.value());
    if (timing.paint_timing->first_paint &&
        timing.paint_timing->first_paint < foreground_duration) {
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramSubresourceFilterForegroundDurationAfterPaint,
          foreground_duration.value() -
              timing.paint_timing->first_paint.value());
    }
  }
}
