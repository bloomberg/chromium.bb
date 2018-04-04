// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_types.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

class Browser;

namespace arc {
class ArcNavigationThrottle;
}

namespace content {
class NavigationHandle;
}  // namespace content

namespace chromeos {

// Allows navigation to be routed to an installed app on Chrome OS, and provides
// a static method for showing an intent picker for the current URL to display
// any handling apps.
class AppsNavigationThrottle : public content::NavigationThrottle {
 public:
  // Restricts the amount of apps displayed to the user without the need of a
  // ScrollView.
  enum { kMaxAppResults = 3 };

  // Possibly creates a navigation throttle that checks if any installed ARC
  // apps can handle the URL being navigated to. The user is prompted if they
  // wish to open the app or remain in the browser.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  // Queries for ARC apps which can handle |url|, and displays the intent picker
  // bubble in |browser|.
  static void ShowIntentPickerBubble(const Browser* browser, const GURL& url);

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
  static void ShowIntentPickerBubbleForApps(
      const Browser* browser,
      const GURL& url,
      const std::vector<IntentPickerAppInfo>& apps);
  void CancelNavigation();

  content::NavigationThrottle::ThrottleCheckResult HandleRequest();

  // Passed as a callback to allow app-platform-specific code to asychronously
  // inform this object of the apps which can handle this URL, and optionally
  // request that the navigation be completely cancelled (e.g. if a preferred
  // app has been opened).
  void OnDeferredRequestProcessed(AppsNavigationAction action,
                                  const std::vector<IntentPickerAppInfo>& apps);

  // A reference to the starting GURL.
  GURL starting_url_;

  std::unique_ptr<arc::ArcNavigationThrottle> arc_throttle_;

  // Keeps track of whether we already shown the UI or preferred app. Since
  // AppsNavigationThrottle cannot wait for the user (due to the non-blocking
  // nature of the feature) the best we can do is check if we launched a
  // preferred app or asked the UI to be shown, this flag ensures we never
  // trigger the UI twice for the same throttle.
  bool ui_displayed_;

  base::WeakPtrFactory<AppsNavigationThrottle> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppsNavigationThrottle);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
