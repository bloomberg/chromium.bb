// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subframe_navigation_filtering_throttle.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/browser/navigation_handle.h"

namespace subresource_filter {

SubframeNavigationFilteringThrottle::SubframeNavigationFilteringThrottle(
    content::NavigationHandle* handle,
    AsyncDocumentSubresourceFilter* parent_frame_filter)
    : content::NavigationThrottle(handle),
      parent_frame_filter_(parent_frame_filter),
      weak_ptr_factory_(this) {
  DCHECK(!handle->IsInMainFrame());
  DCHECK(parent_frame_filter_);
}

SubframeNavigationFilteringThrottle::~SubframeNavigationFilteringThrottle() {
  if (disallowed_) {
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Disallowed",
        total_defer_time_, base::TimeDelta::FromMicroseconds(1),
        base::TimeDelta::FromSeconds(10), 50);
  } else {
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Allowed",
        total_defer_time_, base::TimeDelta::FromMicroseconds(1),
        base::TimeDelta::FromSeconds(10), 50);
  }
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillStartRequest() {
  return DeferToCalculateLoadPolicy();
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillRedirectRequest() {
  return DeferToCalculateLoadPolicy();
}

const char* SubframeNavigationFilteringThrottle::GetNameForLogging() {
  return "SubframeNavigationFilteringThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::DeferToCalculateLoadPolicy() {
  parent_frame_filter_->GetLoadPolicyForSubdocument(
      navigation_handle()->GetURL(),
      base::Bind(&SubframeNavigationFilteringThrottle::OnCalculatedLoadPolicy,
                 weak_ptr_factory_.GetWeakPtr()));
  last_defer_timestamp_ = base::TimeTicks::Now();
  return content::NavigationThrottle::ThrottleCheckResult::DEFER;
}

void SubframeNavigationFilteringThrottle::OnCalculatedLoadPolicy(
    LoadPolicy policy) {
  DCHECK(!last_defer_timestamp_.is_null());
  total_defer_time_ += base::TimeTicks::Now() - last_defer_timestamp_;
  // TODO(csharrison): Support WouldDisallow pattern and expose the policy for
  // metrics. Also, cancel with BLOCK_AND_COLLAPSE when it is implemented.
  if (policy == LoadPolicy::DISALLOW) {
    disallowed_ = true;
    parent_frame_filter_->ReportDisallowedLoad();
    navigation_handle()->CancelDeferredNavigation(
        content::NavigationThrottle::CANCEL);
  } else {
    navigation_handle()->Resume();
  }
}

}  // namespace subresource_filter
