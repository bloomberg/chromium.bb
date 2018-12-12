// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_GOOGLE_PASSWORD_MANAGER_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_PASSWORDS_GOOGLE_PASSWORD_MANAGER_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

// A NavigationThrottle that opens the local Chrome Settings Passwords page when
// there is any issue opening the remote Google Password Manager.
class GooglePasswordManagerNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Returns a NavigationThrottle when we are navigating to the Google Password
  // Manager via a link and the active profile should manage their passwords in
  // the Google Password Manager.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  using content::NavigationThrottle::NavigationThrottle;
  ~GooglePasswordManagerNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillFailRequest() override;
  const char* GetNameForLogging() override;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_GOOGLE_PASSWORD_MANAGER_NAVIGATION_THROTTLE_H_
