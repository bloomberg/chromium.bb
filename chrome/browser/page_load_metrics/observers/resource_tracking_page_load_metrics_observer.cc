// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/resource_tracking_page_load_metrics_observer.h"

namespace page_load_metrics {

ResourceTrackingPageLoadMetricsObserver::
    ResourceTrackingPageLoadMetricsObserver()
    : started_count_(0), completed_count_(0) {}
ResourceTrackingPageLoadMetricsObserver::
    ~ResourceTrackingPageLoadMetricsObserver() {}

void ResourceTrackingPageLoadMetricsObserver::OnStartedResource(
    const ExtraRequestStartInfo& extra_request_start_info) {
  // TODO(petewiL): Store this by type.
  ++started_count_;
}

void ResourceTrackingPageLoadMetricsObserver::OnLoadedResource(
    const ExtraRequestCompleteInfo& extra_request_complete_info) {
  // TODO(petewil): Check to see if the type of the request changed.  If it did,
  // update the old and new types for the started type.  Then update by type for
  // the completed type.
  ++completed_count_;
}

void ResourceTrackingPageLoadMetricsObserver::GetCountsForTypeForTesting(
    const content::ResourceType type,
    int64_t* started_count,
    int64_t* completed_count) {
  if (started_count != nullptr)
    *started_count = started_count_;
  if (completed_count != nullptr)
    *completed_count = completed_count_;
}

}  // namespace page_load_metrics
