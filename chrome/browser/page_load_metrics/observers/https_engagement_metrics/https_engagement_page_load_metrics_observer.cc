// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service_factory.h"
#include "url/url_constants.h"

namespace internal {
const char kHttpsEngagementHistogram[] = "Navigation.EngagementTime.HTTPS";
const char kHttpEngagementHistogram[] = "Navigation.EngagementTime.HTTP";
}

HttpsEngagementPageLoadMetricsObserver::HttpsEngagementPageLoadMetricsObserver(
    content::BrowserContext* context)
    : currently_in_foreground_(false) {
  engagement_service_ =
      HttpsEngagementServiceFactory::GetForBrowserContext(context);
}

void HttpsEngagementPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (started_in_foreground)
    OnShown();
}

void HttpsEngagementPageLoadMetricsObserver::OnHidden() {
  if (!currently_in_foreground_)
    return;
  foreground_time_ += base::TimeTicks::Now() - last_time_shown_;
  currently_in_foreground_ = false;
}

void HttpsEngagementPageLoadMetricsObserver::OnShown() {
  last_time_shown_ = base::TimeTicks::Now();
  currently_in_foreground_ = true;
}

void HttpsEngagementPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!extra_info.committed_url.is_valid() || !extra_info.time_to_commit)
    return;

  // Don't record anything if the user never saw it.
  if (!currently_in_foreground_ && foreground_time_.is_zero())
    return;

  if (currently_in_foreground_)
    OnHidden();

  if (extra_info.committed_url.SchemeIs(url::kHttpsScheme)) {
    if (engagement_service_)
      engagement_service_->RecordTimeOnPage(foreground_time_,
                                            HttpsEngagementService::HTTPS);
    UMA_HISTOGRAM_LONG_TIMES_100(internal::kHttpsEngagementHistogram,
                                 foreground_time_);
  } else if (extra_info.committed_url.SchemeIs(url::kHttpScheme)) {
    if (engagement_service_)
      engagement_service_->RecordTimeOnPage(foreground_time_,
                                            HttpsEngagementService::HTTP);
    UMA_HISTOGRAM_LONG_TIMES_100(internal::kHttpEngagementHistogram,
                                 foreground_time_);
  }
}
