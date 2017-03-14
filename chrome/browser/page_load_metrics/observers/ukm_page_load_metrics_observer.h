// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_UKM_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_UKM_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "net/nqe/network_quality_estimator.h"
#include "ui/base/page_transition_types.h"

namespace content {
class WebContents;
}

namespace internal {

// Name constants are exposed here so they can be referenced from tests.
extern const char kUkmPageLoadEventName[];
extern const char kUkmParseStartName[];
extern const char kUkmDomContentLoadedName[];
extern const char kUkmLoadEventName[];
extern const char kUkmFirstContentfulPaintName[];
extern const char kUkmFirstMeaningfulPaintName[];
extern const char kUkmForegroundDurationName[];
extern const char kUkmFailedProvisionaLoadName[];
extern const char kUkmNetErrorCode[];
extern const char kUkmEffectiveConnectionType[];
extern const char kUkmPageTransition[];

}  // namespace internal

// If URL-Keyed-Metrics (UKM) is enabled in the system, this is used to
// populate it with top-level page-load metrics.
class UkmPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Returns a UkmPageLoadMetricsObserver, or nullptr if it is not needed.
  static std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
  CreateIfNeeded(content::WebContents* web_contents);

  explicit UkmPageLoadMetricsObserver(
      net::NetworkQualityEstimator::NetworkQualityProvider*
          network_quality_provider);
  ~UkmPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;

  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;

  ObservePolicy OnHidden(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;

  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

  void OnComplete(const page_load_metrics::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;

 private:
  // Records page load timing related metrics available in PageLoadTiming, such
  // as first contentful paint.
  void RecordTimingMetrics(const page_load_metrics::PageLoadTiming& timing);

  // Records metrics based on the PageLoadExtraInfo struct, as well as updating
  // the URL. |app_background_time| should be set to a timestamp if the app was
  // backgrounded, otherwise it should be set to a null TimeTicks.
  void RecordPageLoadExtraInfoMetrics(
      const page_load_metrics::PageLoadExtraInfo& info,
      base::TimeTicks app_background_time);

  net::NetworkQualityEstimator::NetworkQualityProvider* const
      network_quality_provider_;

  // Unique UKM identifier for the page load we are recording metrics for.
  const int32_t source_id_;

  net::EffectiveConnectionType effective_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  // PAGE_TRANSITION_LINK is the default PageTransition value.
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;

  DISALLOW_COPY_AND_ASSIGN(UkmPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_UKM_PAGE_LOAD_METRICS_OBSERVER_H_
