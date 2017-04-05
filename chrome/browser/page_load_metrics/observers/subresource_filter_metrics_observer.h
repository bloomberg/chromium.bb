// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_FILTER_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_FILTER_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramSubresourceFilterFirstContentfulPaint[];
extern const char kHistogramSubresourceFilterParseStartToFirstContentfulPaint[];
extern const char kHistogramSubresourceFilterFirstMeaningfulPaint[];
extern const char kHistogramSubresourceFilterParseStartToFirstMeaningfulPaint[];

extern const char kHistogramSubresourceFilterTotalResources[];
extern const char kHistogramSubresourceFilterNetworkResources[];
extern const char kHistogramSubresourceFilterCacheResources[];
extern const char kHistogramSubresourceFilterTotalBytes[];
extern const char kHistogramSubresourceFilterNetworkBytes[];
extern const char kHistogramSubresourceFilterCacheBytes[];

extern const char kHistogramSubresourceFilterMediaTotalResources[];
extern const char kHistogramSubresourceFilterMediaNetworkResources[];
extern const char kHistogramSubresourceFilterMediaCacheResources[];
extern const char kHistogramSubresourceFilterMediaTotalBytes[];
extern const char kHistogramSubresourceFilterMediaNetworkBytes[];
extern const char kHistogramSubresourceFilterMediaCacheBytes[];

extern const char kHistogramSubresourceFilterNoMediaTotalResources[];
extern const char kHistogramSubresourceFilterNoMediaNetworkResources[];
extern const char kHistogramSubresourceFilterNoMediaCacheResources[];
extern const char kHistogramSubresourceFilterNoMediaTotalBytes[];
extern const char kHistogramSubresourceFilterNoMediaNetworkBytes[];
extern const char kHistogramSubresourceFilterNoMediaCacheBytes[];

extern const char kHistogramSubresourceFilterDomContentLoaded[];
extern const char kHistogramSubresourceFilterLoad[];

extern const char kHistogramSubresourceFilterParseDuration[];
extern const char kHistogramSubresourceFilterParseBlockedOnScriptLoad[];
extern const char
    kHistogramSubresourceFilterParseBlockedOnScriptLoadDocumentWrite[];
extern const char kHistogramSubresourceFilterParseBlockedOnScriptExecution[];
extern const char
    kHistogramSubresourceFilterParseBlockedOnScriptExecutionDocumentWrite[];

extern const char kHistogramSubresourceFilterForegroundDuration[];
extern const char kHistogramSubresourceFilterForegroundDurationAfterPaint[];

extern const char kHistogramSubresourceFilterCount[];

extern const char kHistogramSubresourceFilterActivationDecision[];
extern const char kHistogramSubresourceFilterActivationDecisionReload[];

}  // namespace internal

class SubresourceFilterMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SubresourceFilterMetricsObserver() = default;
  ~SubresourceFilterMetricsObserver() override = default;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnComplete(const page_load_metrics::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadedResource(
      const page_load_metrics::ExtraRequestInfo& extra_request_info) override;
  void OnParseStop(const page_load_metrics::PageLoadTiming& timing,
                   const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstMeaningfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnLoadEventStart(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnLoadingBehaviorObserved(
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      bool is_in_main_frame) override;

 private:
  void OnGoingAway(const page_load_metrics::PageLoadTiming& timing,
                   const page_load_metrics::PageLoadExtraInfo& info,
                   base::TimeTicks app_background_time);

  int64_t network_bytes_ = 0;
  int64_t cache_bytes_ = 0;

  int num_network_resources_ = 0;
  int num_cache_resources_ = 0;

  bool subresource_filter_observed_ = false;
  bool played_media_ = false;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_FILTER_METRICS_OBSERVER_H_
