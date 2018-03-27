// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_

#include <memory>

namespace content {
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

namespace chromeos {

// TODO(crbug.com/824598): make this class inherit from
// content::NavigationThrottle, and move the throttle functionality from
// ArcNavigationThrottle into here.
class AppsNavigationThrottle {
 public:
  // Possibly creates a navigation throttle that checks if any installed ARC++
  // apps can handle the URL being navigated to. The user is prompted if they
  // wish to open the app or remain in the browser.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
