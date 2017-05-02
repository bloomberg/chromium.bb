// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/resource_prefetch_predictor_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "content/public/browser/web_contents.h"

namespace internal {

const char kHistogramResourcePrefetchPredictorFirstContentfulPaint[] =
    "PageLoad.Clients.ResourcePrefetchPredictor.PaintTiming."
    "NavigationToFirstContentfulPaint.Prefetchable";
const char kHistogramResourcePrefetchPredictorFirstMeaningfulPaint[] =
    "PageLoad.Clients.ResourcePrefetchPredictor.Experimental.PaintTiming."
    "NavigationToFirstMeaningfulPaint.Prefetchable";

}  // namespace internal

// static
std::unique_ptr<ResourcePrefetchPredictorPageLoadMetricsObserver>
ResourcePrefetchPredictorPageLoadMetricsObserver::CreateIfNeeded(
    content::WebContents* web_contents) {
  predictors::ResourcePrefetchPredictor* predictor =
      predictors::ResourcePrefetchPredictorFactory::GetForProfile(
          web_contents->GetBrowserContext());
  if (!predictor)
    return nullptr;
  return base::MakeUnique<ResourcePrefetchPredictorPageLoadMetricsObserver>(
      predictor, web_contents);
}

ResourcePrefetchPredictorPageLoadMetricsObserver::
    ResourcePrefetchPredictorPageLoadMetricsObserver(
        predictors::ResourcePrefetchPredictor* predictor,
        content::WebContents* web_contents)
    : predictor_(predictor),
      web_contents_(web_contents),
      record_histograms_(false) {
  DCHECK(predictor_);
}

ResourcePrefetchPredictorPageLoadMetricsObserver::
    ~ResourcePrefetchPredictorPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ResourcePrefetchPredictorPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_commited_url,
    bool started_in_foreground) {
  record_histograms_ =
      started_in_foreground &&
      predictor_->IsUrlPrefetchable(navigation_handle->GetURL());

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ResourcePrefetchPredictorPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  record_histograms_ = false;
  return CONTINUE_OBSERVING;
}

void ResourcePrefetchPredictorPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  predictors::NavigationID navigation_id(web_contents_);

  predictor_->RecordFirstContentfulPaint(
      navigation_id, extra_info.navigation_start +
                         timing.paint_timing.first_contentful_paint.value());
  if (record_histograms_) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramResourcePrefetchPredictorFirstContentfulPaint,
        timing.paint_timing.first_contentful_paint.value());
  }
}

void ResourcePrefetchPredictorPageLoadMetricsObserver::OnFirstMeaningfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (record_histograms_) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramResourcePrefetchPredictorFirstMeaningfulPaint,
        timing.paint_timing.first_meaningful_paint.value());
  }
}
