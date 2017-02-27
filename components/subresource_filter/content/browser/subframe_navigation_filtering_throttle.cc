// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subframe_navigation_filtering_throttle.h"

#include "base/bind.h"
#include "base/logging.h"
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

SubframeNavigationFilteringThrottle::~SubframeNavigationFilteringThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillStartRequest() {
  return DeferToCalculateLoadPolicy();
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillRedirectRequest() {
  return DeferToCalculateLoadPolicy();
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::DeferToCalculateLoadPolicy() {
  parent_frame_filter_->GetLoadPolicyForSubdocument(
      navigation_handle()->GetURL(),
      base::Bind(&SubframeNavigationFilteringThrottle::OnCalculatedLoadPolicy,
                 weak_ptr_factory_.GetWeakPtr()));
  return content::NavigationThrottle::ThrottleCheckResult::DEFER;
}

void SubframeNavigationFilteringThrottle::OnCalculatedLoadPolicy(
    LoadPolicy policy) {
  // TODO(csharrison): Support WouldDisallow pattern and expose the policy for
  // metrics. Also, cancel with BLOCK_AND_COLLAPSE when it is implemented.
  if (policy == LoadPolicy::DISALLOW) {
    parent_frame_filter_->ReportDisallowedLoad();
    navigation_handle()->CancelDeferredNavigation(
        content::NavigationThrottle::CANCEL);
  } else {
    navigation_handle()->Resume();
  }
}

}  // namespace subresource_filter
