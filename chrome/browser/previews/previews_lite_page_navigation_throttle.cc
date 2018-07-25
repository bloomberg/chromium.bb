// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"

PreviewsLitePageNavigationThrottle::PreviewsLitePageNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {
  DCHECK(handle);
}

PreviewsLitePageNavigationThrottle::~PreviewsLitePageNavigationThrottle() =
    default;

// static
std::unique_ptr<content::NavigationThrottle>
PreviewsLitePageNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  DCHECK(handle);
  return nullptr;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillStartRequest() {
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillRedirectRequest() {
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillFailRequest() {
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillProcessResponse() {
  return content::NavigationThrottle::PROCEED;
}

const char* PreviewsLitePageNavigationThrottle::GetNameForLogging() {
  return "PreviewsLitePageNavigationThrottle";
}
