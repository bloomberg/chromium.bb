// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace arc {

namespace {

// Searches for a preferred app in |app_candidates| and returns its index. If
// not found, returns |app_candidates.size()|.
size_t FindPreferredApp(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates,
    const GURL& url_for_logging) {
  for (size_t i = 0; i < app_candidates.size(); ++i) {
    if (!app_candidates[i]->is_preferred)
      continue;
    if (ArcIntentHelperBridge::IsIntentHelperPackage(
            app_candidates[i]->package_name)) {
      // If Chrome browser was selected as the preferred app, we shouldn't
      // create a throttle.
      DVLOG(1)
          << "Chrome browser is selected as the preferred app for this URL: "
          << url_for_logging;
    }
    return i;
  }
  return app_candidates.size();  // not found
}

}  // namespace

ArcNavigationThrottle::ArcNavigationThrottle() : weak_ptr_factory_(this) {}

ArcNavigationThrottle::~ArcNavigationThrottle() = default;

bool ArcNavigationThrottle::ShouldDeferRequest(
    content::NavigationHandle* handle,
    chromeos::AppsNavigationCallback callback) {
  ArcServiceManager* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;

  auto* intent_helper_bridge = ArcIntentHelperBridge::GetForBrowserContext(
      handle->GetWebContents()->GetBrowserContext());
  if (!intent_helper_bridge)
    return false;

  const GURL& url = handle->GetURL();
  if (intent_helper_bridge->ShouldChromeHandleUrl(url)) {
    // Allow navigation to proceed if there isn't an android app that handles
    // the given URL.
    return false;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance)
    return false;

  // Return true to defer the navigation until we asynchronously hear back from
  // ARC whether a preferred app should be launched. This makes it safe to bind
  // |handle| as a raw pointer argument. We will either resume or cancel the
  // navigation as soon as the callback is run.
  instance->RequestUrlHandlerList(
      url.spec(),
      base::BindOnce(&ArcNavigationThrottle::OnAppCandidatesReceived,
                     weak_ptr_factory_.GetWeakPtr(), handle,
                     std::move(callback)));
  return true;
}

// static
ArcNavigationThrottle::Platform ArcNavigationThrottle::GetDestinationPlatform(
    const std::string& selected_app_package,
    PickerAction picker_action) {
  switch (picker_action) {
    case PickerAction::ARC_APP_PRESSED:
    case PickerAction::ARC_APP_PREFERRED_PRESSED:
      return Platform::ARC;
    case PickerAction::ERROR:
    case PickerAction::DIALOG_DEACTIVATED:
    case PickerAction::CHROME_PRESSED:
    case PickerAction::CHROME_PREFERRED_PRESSED:
      return Platform::CHROME;
    default:
      return ArcIntentHelperBridge::IsIntentHelperPackage(selected_app_package)
                 ? Platform::CHROME
                 : Platform::ARC;
  }
  NOTREACHED();
  return Platform::SIZE;
}

// static
ArcNavigationThrottle::PickerAction ArcNavigationThrottle::GetPickerAction(
    chromeos::AppType app_type,
    chromeos::IntentPickerCloseReason close_reason,
    bool should_persist) {
  switch (close_reason) {
    case chromeos::IntentPickerCloseReason::ERROR:
      return PickerAction::ERROR;
    case chromeos::IntentPickerCloseReason::DIALOG_DEACTIVATED:
      return PickerAction::DIALOG_DEACTIVATED;
    case chromeos::IntentPickerCloseReason::PREFERRED_APP_FOUND:
      return PickerAction::PREFERRED_ACTIVITY_FOUND;
    case chromeos::IntentPickerCloseReason::STAY_IN_CHROME:
      return should_persist ? PickerAction::CHROME_PREFERRED_PRESSED
                            : PickerAction::CHROME_PRESSED;
    case chromeos::IntentPickerCloseReason::OPEN_APP:
      switch (app_type) {
        case chromeos::AppType::INVALID:
          return PickerAction::INVALID;
        case chromeos::AppType::ARC:
          return should_persist ? PickerAction::ARC_APP_PREFERRED_PRESSED
                                : PickerAction::ARC_APP_PRESSED;
      }
  }

  NOTREACHED();
  return PickerAction::INVALID;
}

void ArcNavigationThrottle::OnAppCandidatesReceived(
    content::NavigationHandle* handle,
    chromeos::AppsNavigationCallback callback,
    std::vector<mojom::IntentHandlerInfoPtr> app_candidates) {
  const GURL& url = handle->GetURL();
  if (!IsAppAvailable(app_candidates)) {
    // This scenario shouldn't be accessed as ArcNavigationThrottle is created
    // iff there are ARC apps which can actually handle the given URL.
    DVLOG(1) << "There are no app candidates for this URL: " << url;
    RecordUma(std::string(), chromeos::AppType::INVALID,
              chromeos::IntentPickerCloseReason::ERROR,
              false /* should_persist */);
    std::move(callback).Run(chromeos::AppsNavigationAction::RESUME, {});
    return;
  }

  content::WebContents* web_contents = handle->GetWebContents();
  // If one of the apps is marked as preferred, launch it immediately.
  if (DidLaunchPreferredArcApp(url, web_contents, app_candidates)) {
    std::move(callback).Run(chromeos::AppsNavigationAction::CANCEL, {});
    return;
  }

  // We are always going to resume navigation at this point, and possibly show
  // the intent picker bubble to prompt the user to choose if they would like to
  // use an ARC app to open the URL.
  ArcAppIconQuery(url, web_contents, std::move(app_candidates),
                  base::BindOnce(std::move(callback),
                                 chromeos::AppsNavigationAction::RESUME));
}

bool ArcNavigationThrottle::DidLaunchPreferredArcApp(
    const GURL& url,
    content::WebContents* web_contents,
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool cancel_navigation = false;
  const size_t index = FindPreferredApp(app_candidates, url);
  if (index != app_candidates.size()) {
    auto close_reason = chromeos::IntentPickerCloseReason::PREFERRED_APP_FOUND;
    const std::string package_name = app_candidates[index]->package_name;

    // Make sure that the instance at least supports HandleUrl.
    auto* arc_service_manager = ArcServiceManager::Get();
    mojom::IntentHelperInstance* instance = nullptr;
    if (arc_service_manager) {
      instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrl);
    }

    if (!instance) {
      close_reason = chromeos::IntentPickerCloseReason::ERROR;
    } else if (ArcIntentHelperBridge::IsIntentHelperPackage(package_name)) {
      chrome::SetIntentPickerViewVisibility(
          chrome::FindBrowserWithWebContents(web_contents), true);
    } else {
      instance->HandleUrl(url.spec(), package_name);
      cancel_navigation = true;
    }
    RecordUma(package_name, chromeos::AppType::ARC, close_reason,
              false /* should_persist */);
  }
  return cancel_navigation;
}

void ArcNavigationThrottle::ArcAppIconQuery(
    const GURL& url,
    content::WebContents* web_contents,
    std::vector<mojom::IntentHandlerInfoPtr> app_candidates,
    chromeos::QueryAppsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* intent_helper_bridge = ArcIntentHelperBridge::GetForBrowserContext(
      web_contents->GetBrowserContext());
  if (!intent_helper_bridge) {
    LOG(ERROR) << "Cannot get an instance of ArcIntentHelperBridge";
    RecordUma(std::string(), chromeos::AppType::INVALID,
              chromeos::IntentPickerCloseReason::ERROR,
              false /* should_persist */);
    std::move(callback).Run({});
    return;
  }
  std::vector<ArcIntentHelperBridge::ActivityName> activities;
  for (const auto& candidate : app_candidates)
    activities.emplace_back(candidate->package_name, candidate->activity_name);

  intent_helper_bridge->GetActivityIcons(
      activities,
      base::BindOnce(&ArcNavigationThrottle::OnAppIconsReceived,
                     chrome::FindBrowserWithWebContents(web_contents),
                     std::move(app_candidates), url, std::move(callback)));
}

// static
void ArcNavigationThrottle::OnAppIconsReceived(
    const Browser* browser,
    std::vector<arc::mojom::IntentHandlerInfoPtr> app_candidates,
    const GURL& url,
    chromeos::QueryAppsCallback callback,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  std::vector<chromeos::IntentPickerAppInfo> app_info;

  for (const auto& candidate : app_candidates) {
    gfx::Image icon;
    const arc::ArcIntentHelperBridge::ActivityName activity(
        candidate->package_name, candidate->activity_name);
    const auto it = icons->find(activity);

    app_info.emplace_back(chromeos::AppType::ARC,
                          it != icons->end() ? it->second.icon16 : gfx::Image(),
                          candidate->package_name, candidate->name);
  }

  std::move(callback).Run(std::move(app_info));
}

// static
void ArcNavigationThrottle::OnIntentPickerClosed(
    const GURL& url,
    const std::string& pkg,
    chromeos::AppType app_type,
    chromeos::IntentPickerCloseReason reason,
    bool should_persist) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);
  }

  if (!instance)
    reason = chromeos::IntentPickerCloseReason::ERROR;

  if (reason == chromeos::IntentPickerCloseReason::STAY_IN_CHROME ||
      (reason == chromeos::IntentPickerCloseReason::OPEN_APP &&
       app_type == chromeos::AppType::ARC)) {
    if (should_persist) {
      DCHECK(arc_service_manager);
      if (ARC_GET_INSTANCE_FOR_METHOD(
              arc_service_manager->arc_bridge_service()->intent_helper(),
              AddPreferredPackage)) {
        instance->AddPreferredPackage(pkg);
      }
    }
  }

  if (reason == chromeos::IntentPickerCloseReason::OPEN_APP)
    instance->HandleUrl(url.spec(), pkg);

  RecordUma(pkg, app_type, reason, should_persist);
}

// static
size_t ArcNavigationThrottle::GetAppIndex(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates,
    const std::string& selected_app_package) {
  for (size_t i = 0; i < app_candidates.size(); ++i) {
    if (app_candidates[i]->package_name == selected_app_package)
      return i;
  }
  return app_candidates.size();
}

// static
void ArcNavigationThrottle::RecordUma(
    const std::string& selected_app_package,
    chromeos::AppType app_type,
    chromeos::IntentPickerCloseReason close_reason,
    bool should_persist) {
  PickerAction action = GetPickerAction(app_type, close_reason, should_persist);
  Platform platform = GetDestinationPlatform(selected_app_package, action);

  UMA_HISTOGRAM_ENUMERATION("Arc.IntentHandlerAction", action,
                            PickerAction::SIZE);

  UMA_HISTOGRAM_ENUMERATION("Arc.IntentHandlerDestinationPlatform", platform,
                            Platform::SIZE);
}

// static
bool ArcNavigationThrottle::IsAppAvailable(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates) {
  return app_candidates.size() > 1 ||
         (app_candidates.size() == 1 &&
          !ArcIntentHelperBridge::IsIntentHelperPackage(
              app_candidates[0]->package_name));
}

// static
bool ArcNavigationThrottle::IsAppAvailableForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates) {
  return IsAppAvailable(app_candidates);
}

// static
size_t ArcNavigationThrottle::FindPreferredAppForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates) {
  return FindPreferredApp(app_candidates, GURL());
}

// static
void ArcNavigationThrottle::QueryArcApps(const Browser* browser,
                                         const GURL& url,
                                         chromeos::QueryAppsCallback callback) {
  arc::ArcServiceManager* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    DVLOG(1) << "Cannot get an instance of ArcServiceManager";
    std::move(callback).Run({});
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance) {
    DVLOG(1) << "Cannot get access to RequestUrlHandlerList";
    std::move(callback).Run({});
    return;
  }

  instance->RequestUrlHandlerList(
      url.spec(),
      base::BindOnce(&ArcNavigationThrottle::AsyncOnAppCandidatesReceived,
                     browser, url, std::move(callback)));
}

// static
void ArcNavigationThrottle::AsyncOnAppCandidatesReceived(
    const Browser* browser,
    const GURL& url,
    chromeos::QueryAppsCallback callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> app_candidates) {
  if (!IsAppAvailable(app_candidates)) {
    DVLOG(1) << "There are no app candidates for this URL";
    std::move(callback).Run({});
    return;
  }

  auto* intent_helper_bridge = arc::ArcIntentHelperBridge::GetForBrowserContext(
      browser->tab_strip_model()->GetActiveWebContents()->GetBrowserContext());
  if (!intent_helper_bridge) {
    DVLOG(1) << "Cannot get an instance of ArcIntentHelperBridge";
    std::move(callback).Run({});
    return;
  }

  std::vector<arc::ArcIntentHelperBridge::ActivityName> activities;
  for (const auto& candidate : app_candidates)
    activities.emplace_back(candidate->package_name, candidate->activity_name);

  intent_helper_bridge->GetActivityIcons(
      activities,
      base::BindOnce(&ArcNavigationThrottle::OnAppIconsReceived, browser,
                     std::move(app_candidates), url, std::move(callback)));
}

}  // namespace arc
