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

constexpr char kBlockTabUnderFormatMessage[] =
    "Chrome stopped this site from navigating to %s, see "
    "https://www.chromestatus.com/feature/5675755719622656 for more details.";

// This class blocks navigations that we've classified as tab-unders. It does so
// by communicating with the popup opener tab helper.
//
// Currently, navigations are considered tab-unders if:
// 1. It is a navigation that is "suspicious"
//    a. It has no user gesture.
//    b. It is renderer-initiated.
//    c. It is cross origin to the last committed URL in the tab.
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

    // Logged at the same time as kBlocked, but will additionally be logged even
    // if the experiment is turned off.
    kDidTabUnder,

    // The user clicked through to navigate to the blocked redirect.
    kClickedThrough,

    // The user did not navigate to the blocked redirect and closed the message.
    // This only gets logged when the user takes action on the UI, not when it
    // gets automatically dismissed by a navigation for example.
    kAcceptedIntervention,

    kCount
  };

  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  ~TabUnderNavigationThrottle() override;

 private:
  explicit TabUnderNavigationThrottle(content::NavigationHandle* handle);

  // This method is described at the top of this file.
  //
  // Note: This method should be robust to navigations at any stage.
  static bool IsSuspiciousClientRedirect(
      content::NavigationHandle* navigation_handle);

  content::NavigationThrottle::ThrottleCheckResult MaybeBlockNavigation();
  void ShowUI();

  bool HasOpenedPopupSinceLastUserGesture() const;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  const char* GetNameForLogging() override;

  // Store whether we're off the record as a member to avoid looking it up all
  // the time.
  const bool off_the_record_ = false;

  // True if the experiment is turned on and the class should actually attempt
  // to block tab-unders.
  const bool block_ = false;

  // Tracks whether this WebContents has opened a popup since the last user
  // gesture, at the time this navigation is starting.
  const bool has_opened_popup_since_last_user_gesture_at_start_ = false;

  // True if the throttle has seen a tab under.
  bool seen_tab_under_ = false;

  DISALLOW_COPY_AND_ASSIGN(TabUnderNavigationThrottle);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_TAB_UNDER_NAVIGATION_THROTTLE_H_
