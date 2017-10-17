// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "content/public/browser/navigation_throttle.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class Browser;

namespace content {
class NavigationHandle;
}  // namespace content

namespace arc {

// A class that allow us to retrieve ARC app's information and handle URL
// traffic initiated on Chrome browser, either on Chrome or an ARC's app.
class ArcNavigationThrottle : public content::NavigationThrottle {
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

  // Restricts the amount of apps displayed to the user without the need of a
  // ScrollView.
  enum { kMaxAppResults = 3 };

  struct AppInfo {
    AppInfo(gfx::Image img, std::string package, std::string activity)
        : icon(img), package_name(package), activity_name(activity) {}
    gfx::Image icon;
    std::string package_name;
    std::string activity_name;
  };

  using QueryAndDisplayArcAppsCallback = base::Callback<void(
      const Browser* browser,
      const std::vector<AppInfo>& app_info,
      const base::Callback<void(const std::string&, CloseReason)>& cb)>;
  explicit ArcNavigationThrottle(content::NavigationHandle* navigation_handle);
  ~ArcNavigationThrottle() override;

  static bool ShouldOverrideUrlLoadingForTesting(const GURL& previous_url,
                                                 const GURL& current_url);

  // Finds |selected_app_package| from the |handlers| array and returns the
  // index. If the app is not found, returns |handlers.size()|.
  static size_t GetAppIndex(
      const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
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
  // Records true if |handlers| contain one or more apps. When this function is
  // called from OnAppCandidatesReceived, |handlers| always contain Chrome (aka
  // intent helper), but the same function doesn't treat this as an app.
  static bool IsAppAvailable(
      const std::vector<mojom::IntentHandlerInfoPtr>& handlers);

  // Swaps Chrome app with any app in row |kMaxAppResults-1| iff its index is
  // bigger, thus ensuring the user can always see Chrome without scrolling.
  // When swap is needed, fills |out_indices| and returns true. If |handlers|
  // do not have Chrome, returns false.
  static bool IsSwapElementsNeeded(
      const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
      std::pair<size_t, size_t>* out_indices);

  static bool IsAppAvailableForTesting(
      const std::vector<mojom::IntentHandlerInfoPtr>& handlers);
  static size_t FindPreferredAppForTesting(
      const std::vector<mojom::IntentHandlerInfoPtr>& handlers);
  static void AsyncShowIntentPickerBubble(const Browser* browser,
                                          const GURL& url);
  // content::Navigation implementation:
  const char* GetNameForLogging() override;

 private:
  // content::Navigation implementation:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;

  NavigationThrottle::ThrottleCheckResult HandleRequest();
  void OnAppCandidatesReceived(
      std::vector<mojom::IntentHandlerInfoPtr> handlers);
  void OnAppIconsReceived(
      std::vector<mojom::IntentHandlerInfoPtr> handlers,
      std::unique_ptr<ArcIntentHelperBridge::ActivityToIconsMap> icons);
  void DisplayArcApps() const;
  GURL GetStartingGURL() const;
  static void AsyncOnAppCandidatesReceived(
      const Browser* browser,
      const GURL& url,
      std::vector<arc::mojom::IntentHandlerInfoPtr> handlers);
  static void AsyncOnAppIconsReceived(
      const Browser* browser,
      std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
      const GURL& url,
      std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons);
  static void AsyncOnIntentPickerClosed(
      const GURL& url,
      const std::string& package_name,
      arc::ArcNavigationThrottle::CloseReason close_reason);

  // Keeps a referrence to the starting GURL.
  GURL starting_gurl_;

  // Keeps track of whether we already shown the UI or preferred app. Since
  // ArcNavigationThrottle cannot wait for the user (due to the non-blocking
  // nature of the feature) the best we can do is check if we launched a
  // preferred app or asked the UI to be shown, this flag ensures we never
  // trigger the UI twice for the same throttle.
  bool ui_displayed_;

  // This has to be the last member of the class.
  base::WeakPtrFactory<ArcNavigationThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcNavigationThrottle);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_NAVIGATION_THROTTLE_H_
