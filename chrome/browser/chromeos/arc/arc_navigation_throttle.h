// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_NAVIGATION_THROTTLE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
#include "content/public/browser/navigation_throttle.h"
#include "ui/gfx/image/image.h"

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
    DIALOG_DEACTIVATED = 1,
    ALWAYS_PRESSED = 2,
    JUST_ONCE_PRESSED = 3,
    PREFERRED_ACTIVITY_FOUND = 4,
    SIZE,
  };

  using NameAndIcon = std::pair<std::string, gfx::Image>;
  using ShowDisambigDialogCallback =
      base::Callback<void(content::NavigationHandle* handle,
                          const std::vector<NameAndIcon>& app_info,
                          const base::Callback<void(size_t, CloseReason)>& cb)>;
  ArcNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      const ShowDisambigDialogCallback& show_disambig_dialog_cb);
  ~ArcNavigationThrottle() override;

 private:
  // content::Navigation implementation:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;

  void OnAppCandidatesReceived(mojo::Array<mojom::UrlHandlerInfoPtr> handlers);
  void OnAppIconsReceived(
      mojo::Array<mojom::UrlHandlerInfoPtr> handlers,
      std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> icons);
  void OnDisambigDialogClosed(mojo::Array<mojom::UrlHandlerInfoPtr> handlers,
                              size_t selected_app_index,
                              CloseReason close_reason);

  // A callback object that allow us to display an IntentPicker when Run() is
  // executed, it also allow us to report the user's selection back to
  // OnDisambigDialogClosed().
  ShowDisambigDialogCallback show_disambig_dialog_callback_;

  base::WeakPtrFactory<ArcNavigationThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcNavigationThrottle);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_NAVIGATION_THROTTLE_H_
