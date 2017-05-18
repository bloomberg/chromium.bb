// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

#include <utility>

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
    const mojom::PageLoadMetadata& main_frame_metadata,
    const mojom::PageLoadMetadata& subframe_metadata)
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
      subframe_metadata(subframe_metadata) {}

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
      base::TimeDelta(), page_load_metrics::mojom::PageLoadMetadata(),
      page_load_metrics::mojom::PageLoadMetadata());
}

ExtraRequestCompleteInfo::ExtraRequestCompleteInfo(
    const GURL& url,
    int frame_tree_node_id,
    bool was_cached,
    int64_t raw_body_bytes,
    int64_t original_network_content_length,
    std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
        data_reduction_proxy_data,
    content::ResourceType detected_resource_type)
    : url(url),
      frame_tree_node_id(frame_tree_node_id),
      was_cached(was_cached),
      raw_body_bytes(raw_body_bytes),
      original_network_content_length(original_network_content_length),
      data_reduction_proxy_data(std::move(data_reduction_proxy_data)),
      resource_type(detected_resource_type) {}

ExtraRequestCompleteInfo::~ExtraRequestCompleteInfo() {}

ExtraRequestStartInfo::ExtraRequestStartInfo(content::ResourceType found_type)
    : resource_type(found_type) {}

ExtraRequestStartInfo::ExtraRequestStartInfo(
    const ExtraRequestStartInfo& other) = default;

ExtraRequestStartInfo::~ExtraRequestStartInfo() {}

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
    const mojom::PageLoadTiming& timing,
    const PageLoadExtraInfo& extra_info) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnShown() {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const mojom::PageLoadTiming& timing,
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
