// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_FILTER_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_FILTER_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_level.h"

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
    : public page_load_metrics::PageLoadMetricsObserver,
      public subresource_filter::SubresourceFilterObserver {
 public:
  SubresourceFilterMetricsObserver();
  ~SubresourceFilterMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;
  void OnParseStop(const page_load_metrics::mojom::PageLoadTiming& timing,
                   const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnLoadingBehaviorObserved(
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      bool is_in_main_frame) override;

 private:
  // subresource_filter::SubresourceFilterObserver:
  void OnSubresourceFilterGoingAway() override;
  void OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      subresource_filter::ActivationDecision activation_decision,
      const subresource_filter::ActivationState& activation_state) override;

  void OnGoingAway(const page_load_metrics::mojom::PageLoadTiming& timing,
                   const page_load_metrics::PageLoadExtraInfo& info,
                   base::TimeTicks app_background_time);

  base::Optional<subresource_filter::ActivationDecision> activation_decision_;
  base::Optional<subresource_filter::ActivationLevel> activation_level_;

  ScopedObserver<subresource_filter::SubresourceFilterObserverManager,
                 subresource_filter::SubresourceFilterObserver>
      scoped_observer_;

  int64_t network_bytes_ = 0;
  int64_t cache_bytes_ = 0;

  int num_network_resources_ = 0;
  int num_cache_resources_ = 0;

  // Used as a unique id for the ongoing navigation. Do not use after OnCommit.
  content::NavigationHandle* navigation_handle_ = nullptr;

  bool subresource_filter_observed_ = false;
  bool played_media_ = false;
  bool did_commit_ = false;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_FILTER_METRICS_OBSERVER_H_
