// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/subresource_filter_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

using subresource_filter::ContentSubresourceFilterDriverFactory;

using ActivationDecision = subresource_filter::
    ContentSubresourceFilterDriverFactory::ActivationDecision;

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

  if (ContentSubresourceFilterDriverFactory::NavigationIsPageReload(
          navigation_handle->GetURL(), navigation_handle->GetReferrer(),
          navigation_handle->GetPageTransition())) {
    UMA_HISTOGRAM_ENUMERATION(
        internal::kHistogramSubresourceFilterActivationDecisionReload,
        static_cast<int>(decision),
        static_cast<int>(ActivationDecision::ACTIVATION_DECISION_MAX));
  }
}

}  // namespace

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceFilterMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record final metrics collected up to this point.
  if (info.did_commit)
    OnGoingAway(timing, info);
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
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  OnGoingAway(timing, info);
}

void SubresourceFilterMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestInfo& extra_request_info) {
  if (extra_request_info.was_cached) {
    ++num_cache_resources_;
    cache_bytes_ += extra_request_info.raw_body_bytes;
  } else {
    ++num_network_resources_;
    network_bytes_ += extra_request_info.raw_body_bytes;
  }
}

void SubresourceFilterMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!subresource_filter_observed_)
    return;

  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSubresourceFilterFirstContentfulPaint,
        timing.first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSubresourceFilterParseStartToFirstContentfulPaint,
        timing.first_contentful_paint.value() - timing.parse_start.value());
  }
}

void SubresourceFilterMetricsObserver::OnFirstMeaningfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!subresource_filter_observed_)
    return;

  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSubresourceFilterFirstMeaningfulPaint,
        timing.first_meaningful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramSubresourceFilterParseStartToFirstMeaningfulPaint,
        timing.first_meaningful_paint.value() - timing.parse_start.value());
  }
}

void SubresourceFilterMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!subresource_filter_observed_)
    return;

  if (WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramSubresourceFilterDomContentLoaded,
                        timing.dom_content_loaded_event_start.value());
  }
}

void SubresourceFilterMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!subresource_filter_observed_)
    return;

  if (WasStartedInForegroundOptionalEventInForeground(timing.load_event_start,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramSubresourceFilterLoad,
                        timing.load_event_start.value());
  }
}

void SubresourceFilterMetricsObserver::OnLoadingBehaviorObserved(
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (subresource_filter_observed_)
    return;

  subresource_filter_observed_ =
      page_load_metrics::DidObserveLoadingBehaviorInAnyFrame(
          info, blink::WebLoadingBehaviorFlag::
                    WebLoadingBehaviorSubresourceFilterMatch);

  if (subresource_filter_observed_) {
    UMA_HISTOGRAM_BOOLEAN(internal::kHistogramSubresourceFilterCount, true);
  }
}

void SubresourceFilterMetricsObserver::OnGoingAway(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
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
}
