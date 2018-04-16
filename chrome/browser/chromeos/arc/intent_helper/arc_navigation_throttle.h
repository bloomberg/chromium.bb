// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_types.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "url/gurl.h"

class Browser;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace arc {

// A class that allow us to retrieve ARC app's information and handle URL
// traffic initiated on Chrome browser, either on Chrome or an ARC's app.
class ArcNavigationThrottle {
 public:
  ArcNavigationThrottle();
  ~ArcNavigationThrottle();

  // Returns true if the navigation request represented by |handle| should be
  // deferred while ARC is queried for apps, and if so, |callback| will be run
  // asynchronously with the action for the navigation. |callback| will not be
  // run if false is returned.
  bool ShouldDeferRequest(content::NavigationHandle* handle,
                          chromeos::AppsNavigationCallback callback);

  // Finds |selected_app_package| from the |app_candidates| array and returns
  // the index. If the app is not found, returns |app_candidates.size()|.
  static size_t GetAppIndex(
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates,
      const std::string& selected_app_package);

  // TODO(djacobo): Remove this function and instead stop ARC from returning
  // Chrome as a valid app candidate.
  // Records true if |app_candidates| contain one or more apps. When this
  // function is called from OnAppCandidatesReceived, |app_candidates| always
  // contains Chrome (aka intent helper), but the same function doesn't treat
  // this as an app.
  static bool IsAppAvailable(
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates);

  static bool IsAppAvailableForTesting(
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates);
  static size_t FindPreferredAppForTesting(
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates);
  static void QueryArcApps(const Browser* browser,
                           const GURL& url,
                           chromeos::QueryAppsCallback callback);

  // Called to launch an ARC app if it was selected by the user, and persist the
  // preference to launch or stay in Chrome if |should_persist| is true. Returns
  // true if an app was launched, and false otherwise.
  static bool MaybeLaunchOrPersistArcApp(const GURL& url,
                                         const std::string& package_name,
                                         bool should_launch,
                                         bool should_persist);

 private:
  // Determines whether we should open a preferred app or show the intent
  // picker. Resume/Cancel the navigation which was put in DEFER. Close the
  // current tab only if we continue the navigation on ARC and the current tab
  // was explicitly generated for this navigation.
  void OnAppCandidatesReceived(
      content::NavigationHandle* handle,
      chromeos::AppsNavigationCallback callback,
      std::vector<mojom::IntentHandlerInfoPtr> app_candidates);

  // Returns true if an app in |app_candidates| is preferred for handling the
  // navigation represented by |handle|, and we are successfully able to launch
  // it.
  bool DidLaunchPreferredArcApp(
      const GURL& url,
      content::WebContents* web_contents,
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates);

  // Queries the ArcIntentHelperBridge for ARC app icons for the apps in
  // |app_candidates|. Calls OnAppIconsReceived() when finished.
  void ArcAppIconQuery(const GURL& url,
                       content::WebContents* web_contents,
                       std::vector<mojom::IntentHandlerInfoPtr> app_candidates,
                       chromeos::QueryAppsCallback callback);

  static void AsyncOnAppCandidatesReceived(
      const Browser* browser,
      const GURL& url,
      chromeos::QueryAppsCallback callback,
      std::vector<arc::mojom::IntentHandlerInfoPtr> app_candidates);

  static void OnAppIconsReceived(
      const Browser* browser,
      std::vector<arc::mojom::IntentHandlerInfoPtr> app_candidates,
      const GURL& url,
      chromeos::QueryAppsCallback callback,
      std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons);

  // This has to be the last member of the class.
  base::WeakPtrFactory<ArcNavigationThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcNavigationThrottle);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_NAVIGATION_THROTTLE_H_
