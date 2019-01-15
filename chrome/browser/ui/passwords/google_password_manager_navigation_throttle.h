// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_GOOGLE_PASSWORD_MANAGER_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_PASSWORDS_GOOGLE_PASSWORD_MANAGER_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

// A NavigationThrottle that opens the local Chrome Settings Passwords page when
// there is any issue opening the remote Google Password Manager.
class GooglePasswordManagerNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Records the result of a throttled navigation to the Google Password
  // Manager.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class NavigationResult {
    // The navigation succeeded before timing out.
    kSuccess = 0,
    // The navigation failed before timing out.
    kFailure = 1,
    // The navigation timed out.
    kTimeout = 2,
    kMaxValue = kTimeout,
  };

  // Returns a NavigationThrottle when we are navigating to the Google Password
  // Manager via a link and the active profile should manage their passwords in
  // the Google Password Manager.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  using content::NavigationThrottle::NavigationThrottle;
  ~GooglePasswordManagerNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  void OnTimeout();

  base::OneShotTimer timer_;
  base::TimeTicks navigation_start_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_GOOGLE_PASSWORD_MANAGER_NAVIGATION_THROTTLE_H_
