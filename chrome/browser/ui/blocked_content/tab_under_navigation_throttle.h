// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_TAB_UNDER_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_TAB_UNDER_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}

// This class blocks navigations that we've classified as tab-unders. It does so
// by communicating with the popup opener tab helper.
//
// Currently, navigations are considered tab-unders if:
// 1. It is a navigation that is "suspicious"
//    a. It starts when the tab is in the background.
//    b. It has no user gesture.
//    c. It is renderer-initiated.
//    d. It is cross origin to the last committed URL in the tab.
// 2. The tab has opened a popup and hasn't received a user gesture since then.
//    This information is tracked by the PopupOpenerTabHelper.
class TabUnderNavigationThrottle : public content::NavigationThrottle {
 public:
  static const base::Feature kBlockTabUnders;

  // This enum backs a histogram. Update enums.xml if you make any updates, and
  // put new entries before |kLast|.
  enum class Action {
    // Logged at WillStartRequest.
    kStarted,

    // Logged when a navigation is blocked.
    kBlocked,

    kCount
  };

  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  // This method is described at the top of this file.
  //
  // Note: Pass in |started_in_background| because depending on the state the
  // navigation is in, we need additional data to determine whether it started
  // in the background.
  //
  // Note: This method should be robust to navigations at any stage.
  static bool IsSuspiciousClientRedirect(
      content::NavigationHandle* navigation_handle,
      bool started_in_background);

  ~TabUnderNavigationThrottle() override;

 private:
  explicit TabUnderNavigationThrottle(content::NavigationHandle* handle);

  content::NavigationThrottle::ThrottleCheckResult MaybeBlockNavigation();

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  const char* GetNameForLogging() override;

  void LogAction(Action) const;

  bool started_in_background_ = false;

  DISALLOW_COPY_AND_ASSIGN(TabUnderNavigationThrottle);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_TAB_UNDER_NAVIGATION_THROTTLE_H_
