// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_navigation_throttle.h"

#include "components/subresource_filter/content/browser/content_subresource_filter_driver.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/browser/navigation_handle.h"

namespace subresource_filter {

// static
std::unique_ptr<content::NavigationThrottle>
SubresourceFilterNavigationThrottle::Create(content::NavigationHandle* handle) {
  return std::unique_ptr<content::NavigationThrottle>(
      new SubresourceFilterNavigationThrottle(handle));
}

SubresourceFilterNavigationThrottle::SubresourceFilterNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

SubresourceFilterNavigationThrottle::~SubresourceFilterNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterNavigationThrottle::WillProcessResponse() {
  if (!navigation_handle()->GetURL().SchemeIsHTTPOrHTTPS())
    return NavigationThrottle::PROCEED;

  ContentSubresourceFilterDriverFactory::FromWebContents(
      navigation_handle()->GetWebContents())
      ->ReadyToCommitMainFrameNavigation(
          navigation_handle()->GetRenderFrameHost(),
          navigation_handle()->GetURL());

  return NavigationThrottle::PROCEED;
}

}  // namespace subresource_filter
