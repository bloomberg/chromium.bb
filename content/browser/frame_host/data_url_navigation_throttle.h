// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_DATA_URL_NAVIGATION_THROTTLE_
#define CONTENT_BROWSER_FRAME_HOST_DATA_URL_NAVIGATION_THROTTLE_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

class DataUrlNavigationThrottle : public NavigationThrottle {
 public:
  explicit DataUrlNavigationThrottle(NavigationHandle* navigation_handle);
  ~DataUrlNavigationThrottle() override;

  // NavigationThrottle method:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  static std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      NavigationHandle* navigation_handle);

 private:
  DISALLOW_COPY_AND_ASSIGN(DataUrlNavigationThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_DATA_URL_NAVIGATION_THROTTLE_
