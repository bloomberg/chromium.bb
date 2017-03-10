// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace page_load_metrics {

PageLoadExtraInfo::PageLoadExtraInfo(
    base::TimeTicks navigation_start,
    const base::Optional<base::TimeDelta>& first_background_time,
    const base::Optional<base::TimeDelta>& first_foreground_time,
    bool started_in_foreground,
    UserInitiatedInfo user_initiated_info,
    const GURL& url,
    const GURL& start_url,
    bool did_commit,
    PageEndReason page_end_reason,
    UserInitiatedInfo page_end_user_initiated_info,
    const base::Optional<base::TimeDelta>& page_end_time,
    const PageLoadMetadata& main_frame_metadata,
    const PageLoadMetadata& child_frame_metadata)
    : navigation_start(navigation_start),
      first_background_time(first_background_time),
      first_foreground_time(first_foreground_time),
      started_in_foreground(started_in_foreground),
      user_initiated_info(user_initiated_info),
      url(url),
      start_url(start_url),
      did_commit(did_commit),
      page_end_reason(page_end_reason),
      page_end_user_initiated_info(page_end_user_initiated_info),
      page_end_time(page_end_time),
      main_frame_metadata(main_frame_metadata),
      child_frame_metadata(child_frame_metadata) {}

PageLoadExtraInfo::PageLoadExtraInfo(const PageLoadExtraInfo& other) = default;

PageLoadExtraInfo::~PageLoadExtraInfo() {}

// static
PageLoadExtraInfo PageLoadExtraInfo::CreateForTesting(
    const GURL& url,
    bool started_in_foreground) {
  return PageLoadExtraInfo(
      base::TimeTicks::Now() /* navigation_start */,
      base::Optional<base::TimeDelta>() /* first_background_time */,
      base::Optional<base::TimeDelta>() /* first_foreground_time */,
      started_in_foreground /* started_in_foreground */,
      UserInitiatedInfo::BrowserInitiated(), url, url, true /* did_commit */,
      page_load_metrics::END_NONE,
      page_load_metrics::UserInitiatedInfo::NotUserInitiated(),
      base::TimeDelta(), page_load_metrics::PageLoadMetadata(),
      page_load_metrics::PageLoadMetadata());
}

ExtraRequestInfo::ExtraRequestInfo(bool was_cached,
                                   int64_t raw_body_bytes,
                                   bool data_reduction_proxy_used,
                                   int64_t original_network_content_length)
    : was_cached(was_cached),
      raw_body_bytes(raw_body_bytes),
      data_reduction_proxy_used(data_reduction_proxy_used),
      original_network_content_length(original_network_content_length) {}

ExtraRequestInfo::ExtraRequestInfo(const ExtraRequestInfo& other) = default;

ExtraRequestInfo::~ExtraRequestInfo() {}

FailedProvisionalLoadInfo::FailedProvisionalLoadInfo(base::TimeDelta interval,
                                                     net::Error error)
    : time_to_failed_provisional_load(interval), error(error) {}

FailedProvisionalLoadInfo::~FailedProvisionalLoadInfo() {}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnHidden(
    const PageLoadTiming& timing,
    const PageLoadExtraInfo& extra_info) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnShown() {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const PageLoadTiming& timing,
    const PageLoadExtraInfo& extra_info) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  return mime_type == "text/html" || mime_type == "application/xhtml+xml"
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

}  // namespace page_load_metrics
