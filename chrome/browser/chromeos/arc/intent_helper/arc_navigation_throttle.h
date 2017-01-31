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

namespace content {
class NavigationHandle;
class WebContents;
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
    DIALOG_DEACTIVATED = 1,
    ALWAYS_PRESSED = 2,
    JUST_ONCE_PRESSED = 3,
    PREFERRED_ACTIVITY_FOUND = 4,
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

  using ShowIntentPickerCallback = base::Callback<void(
      content::WebContents* web_contents,
      const std::vector<AppInfo>& app_info,
      const base::Callback<void(const std::string&, CloseReason)>& cb)>;
  ArcNavigationThrottle(content::NavigationHandle* navigation_handle,
                        const ShowIntentPickerCallback& show_intent_picker_cb);
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
  void OnIntentPickerClosed(std::vector<mojom::IntentHandlerInfoPtr> handlers,
                            const std::string& selected_app_package,
                            CloseReason close_reason);
  GURL GetStartingGURL() const;
  // A callback object that allow us to display an IntentPicker when Run() is
  // executed, it also allow us to report the user's selection back to
  // OnIntentPickerClosed().
  ShowIntentPickerCallback show_intent_picker_callback_;

  // A cache of the action the user took the last time this navigation throttle
  // popped up the intent picker dialog.  If the dialog has never been popped up
  // before, this will have a value of CloseReason::INVALID.  Used to avoid
  // popping up the dialog multiple times on chains of multiple redirects.
  CloseReason previous_user_action_;

  // Keeps a referrence to the starting GURL.
  GURL starting_gurl_;

  // This has to be the last member of the class.
  base::WeakPtrFactory<ArcNavigationThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcNavigationThrottle);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_NAVIGATION_THROTTLE_H_
