// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NOSCRIPT_PREVIEW_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NOSCRIPT_PREVIEW_PAGE_LOAD_METRICS_OBSERVER_H_

#include <stdint.h>

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace content {
class NavigationHandle;
}

namespace previews {

namespace noscript_preview_names {

extern const char kNavigationToLoadEvent[];
extern const char kNavigationToFirstContentfulPaint[];
extern const char kNavigationToFirstMeaningfulPaint[];
extern const char kParseBlockedOnScriptLoad[];
extern const char kParseDuration[];

extern const char kNumNetworkResources[];
extern const char kNetworkBytes[];

}  // namespace noscript_preview_names

// Observer responsible for recording page load metrics when a NoScript Preview
// is loaded in the page.
class NoScriptPreviewPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  NoScriptPreviewPageLoadMetricsObserver();
  ~NoScriptPreviewPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnDataUseObserved(int64_t received_data_length,
                         int64_t data_reduction_proxy_bytes_saved) override;

 protected:
  // Virtual for testing. Writes the savings to the data saver feature.
  virtual void WriteToSavings(const GURL& url, int64_t byte_savings);

 private:
  void RecordTimingMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info);

  // Records UMA of page size when the observer is about to be deleted.
  void RecordPageSizeUMA() const;

  content::BrowserContext* browser_context_;

  // The total number of bytes from OnDataUseObserved().
  int64_t total_network_bytes_ = 0;

  // The percent of bytes used by load event that should be considered savings.
  // This is often larger than 100 as it corresponds to bytes that were not
  // downloaded.
  int data_savings_inflation_percent_ = 0;

  int64_t num_network_resources_ = 0;
  int64_t network_bytes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NoScriptPreviewPageLoadMetricsObserver);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NOSCRIPT_PREVIEW_PAGE_LOAD_METRICS_OBSERVER_H_
