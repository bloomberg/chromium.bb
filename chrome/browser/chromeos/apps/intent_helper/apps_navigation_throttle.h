// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace arc {
class ArcNavigationThrottle;
}

namespace content {
class NavigationHandle;
}  // namespace content

namespace chromeos {

enum class AppsNavigationAction;

// Allows navigation to be routed to an installed app on Chrome OS.
class AppsNavigationThrottle : public content::NavigationThrottle {
 public:
  // Possibly creates a navigation throttle that checks if any installed ARC++
  // apps can handle the URL being navigated to. The user is prompted if they
  // wish to open the app or remain in the browser.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  static bool ShouldOverrideUrlLoadingForTesting(const GURL& previous_url,
                                                 const GURL& current_url);

  explicit AppsNavigationThrottle(content::NavigationHandle* navigation_handle);
  ~AppsNavigationThrottle() override;

  // content::NavigationHandle overrides
  const char* GetNameForLogging() override;
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;

 private:
  void CancelNavigation();

  content::NavigationThrottle::ThrottleCheckResult HandleRequest();

  // Passed as a callback to allow app-platform-specific code to asychronously
  // inform this object of the result of whether a deferred request should be
  // resumed or canceled.
  void OnDeferredRequestProcessed(AppsNavigationAction action);

  // A reference to the starting GURL.
  GURL starting_url_;

  std::unique_ptr<arc::ArcNavigationThrottle> arc_throttle_;

  // Keeps track of whether we already shown the UI or preferred app. Since
  // ArcNavigationThrottle cannot wait for the user (due to the non-blocking
  // nature of the feature) the best we can do is check if we launched a
  // preferred app or asked the UI to be shown, this flag ensures we never
  // trigger the UI twice for the same throttle.
  bool ui_displayed_;

  base::WeakPtrFactory<AppsNavigationThrottle> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppsNavigationThrottle);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
