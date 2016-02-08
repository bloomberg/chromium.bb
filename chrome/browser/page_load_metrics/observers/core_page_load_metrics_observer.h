// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CORE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CORE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

// NOTE: Some of these histograms are separated into a separate histogram
// specified by the ".Background" suffix. For these events, we put them into the
// background histogram if the web contents was ever in the background from
// navigation start to the event in question.
extern const char kHistogramCommit[];
extern const char kHistogramFirstLayout[];
extern const char kHistogramFirstTextPaint[];
extern const char kHistogramDomContentLoaded[];
extern const char kHistogramLoad[];
extern const char kHistogramFirstPaint[];
extern const char kHistogramFirstImagePaint[];
extern const char kHistogramFirstContentfulPaint[];
extern const char kBackgroundHistogramCommit[];
extern const char kBackgroundHistogramFirstLayout[];
extern const char kBackgroundHistogramFirstTextPaint[];
extern const char kBackgroundHistogramDomContentLoaded[];
extern const char kBackgroundHistogramLoad[];
extern const char kBackgroundHistogramFirstPaint[];
extern const char kBackgroundHistogramFirstImagePaint[];
extern const char kBackgroundHistogramFirstContentfulPaint[];

extern const char kHistogramFirstContentfulPaintHigh[];
extern const char kHistogramFirstContentfulPaintLow[];

extern const char kHistogramFirstBackground[];
extern const char kHistogramFirstForeground[];

extern const char kHistogramBackgroundBeforePaint[];
extern const char kHistogramBackgroundBeforeCommit[];
extern const char kHistogramFailedProvisionalLoad[];

extern const char kRapporMetricsNameCoarseTiming[];

}  // namespace internal

// Observer responsible for recording 'core' page load metrics. Core metrics are
// maintained by loading-dev team, typically the metrics under
// PageLoad.Timing2.*.
class CorePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  CorePageLoadMetricsObserver();
  ~CorePageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  void OnComplete(const page_load_metrics::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFailedProvisionalLoad(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Information related to failed provisional loads.
  // Populated in OnFailedProvisionalLoad and accessed in OnComplete.
  struct FailedProvisionalLoadInfo {
    base::TimeDelta interval;
    net::Error error;

    FailedProvisionalLoadInfo() : error(net::OK) {}
  };

  void RecordTimingHistograms(const page_load_metrics::PageLoadTiming& timing,
                              const page_load_metrics::PageLoadExtraInfo& info);
  void RecordRappor(const page_load_metrics::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info);

  FailedProvisionalLoadInfo failed_provisional_load_info_;

  DISALLOW_COPY_AND_ASSIGN(CorePageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CORE_PAGE_LOAD_METRICS_OBSERVER_H_
