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
  // These enums are used to define the buckets for an enumerated UMA histogram
  // and need to be synced with histograms.xml. This enum class should also be
  // treated as append-only.
  enum class CloseReason : int {
    ERROR = 0,
    // DIALOG_DEACTIVATED keeps track of the user dismissing the UI via clicking
    // the close button or clicking outside of the IntentPickerBubbleView
    // surface. As with CHROME_PRESSED, the user stays in Chrome, however we
    // keep both options since CHROME_PRESSED is tied to an explicit intent of
    // staying in Chrome, not only just getting rid of the
    // IntentPickerBubbleView UI.
    DIALOG_DEACTIVATED = 1,
    OBSOLETE_ALWAYS_PRESSED = 2,
    OBSOLETE_JUST_ONCE_PRESSED = 3,
    PREFERRED_ACTIVITY_FOUND = 4,
    // The prefix "CHROME"/"ARC_APP" determines whether the user pressed [Stay
    // in Chrome] or [Use app] at IntentPickerBubbleView respectively.
    // "PREFERRED" denotes when the user decides to save this selection, whether
    // an app or Chrome was selected.
    CHROME_PRESSED = 5,
    CHROME_PREFERRED_PRESSED = 6,
    ARC_APP_PRESSED = 7,
    ARC_APP_PREFERRED_PRESSED = 8,
    SIZE,
    INVALID = SIZE,
  };

  // As for CloseReason, these define the buckets for an UMA histogram, so this
  // must be treated in an append-only fashion. This helps especify where a
  // navigation will continue.
  enum class Platform : int {
    ARC = 0,
    CHROME = 1,
    SIZE,
  };

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
  // Determines the destination of the current navigation. We know that if the
  // |close_reason| is either ERROR or DIALOG_DEACTIVATED the navigation MUST
  // stay in Chrome, otherwise we can assume the navigation goes to ARC with the
  // exception of the |selected_app_package| being Chrome.
  static Platform GetDestinationPlatform(
      const std::string& selected_app_package,
      CloseReason close_reason);
  // Records intent picker usage statistics as well as whether navigations are
  // continued or redirected to Chrome or ARC respectively, via UMA histograms.
  static void RecordUma(CloseReason close_reason, Platform platform);
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
  static void OnIntentPickerClosed(
      const GURL& url,
      const std::string& package_name,
      arc::ArcNavigationThrottle::CloseReason close_reason);

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
