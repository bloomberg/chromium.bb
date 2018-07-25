// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

// This class does the actual decision making about when to serve a Lite Page
// Server Preview, and the legwork to trigger the Preview navigation. When a
// Preview is triggered, it will cancel the incoming navigation and PostTask a
// new one to the Previews Server.
class PreviewsLitePageNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit PreviewsLitePageNavigationThrottle(
      content::NavigationHandle* handle);

  ~PreviewsLitePageNavigationThrottle() override;

  // If a Preview is triggered, this method returns an instance of |this|.
  // Otherwise it will return a nullptr.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

 private:
  // content::NavigationThrottle implementation:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageNavigationThrottle);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_
