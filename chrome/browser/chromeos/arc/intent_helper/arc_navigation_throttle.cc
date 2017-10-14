// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/page_transition_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/image/image.h"

namespace arc {

namespace {

constexpr char kGoogleCom[] = "google.com";

// Compares the host name of the referrer and target URL to decide whether
// the navigation needs to be overriden.
bool ShouldOverrideUrlLoading(const GURL& previous_url,
                              const GURL& current_url) {
  // When the navigation is initiated in a web page where sending a referrer
  // is disabled, |previous_url| can be empty. In this case, we should open
  // it in the desktop browser.
  if (!previous_url.is_valid() || previous_url.is_empty())
    return false;

  // Also check |current_url| just in case.
  if (!current_url.is_valid() || current_url.is_empty()) {
    DVLOG(1) << "Unexpected URL: " << current_url << ", opening it in Chrome.";
    return false;
  }

  // Check the scheme for both |previous_url| and |current_url| since an
  // extension could have referred us (e.g. Google Docs).
  if (!current_url.SchemeIsHTTPOrHTTPS() ||
      !previous_url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  if (net::registry_controlled_domains::SameDomainOrHost(
          current_url, previous_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    if (net::registry_controlled_domains::GetDomainAndRegistry(
            current_url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES) ==
        kGoogleCom) {
      // Navigation within the google.com domain are good candidates for this
      // throttle (and consecuently the picker UI) only if they have different
      // hosts, this is because multiple services are hosted within the same
      // domain e.g. play.google.com, mail.google.com and so on.
      return current_url.host_piece() != previous_url.host_piece();
    }

    return false;
  }
  return true;
}

// Searches for a preferred app in |handlers| and returns its index. If not
// found, returns |handlers.size()|.
size_t FindPreferredApp(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    const GURL& url_for_logging) {
  for (size_t i = 0; i < handlers.size(); ++i) {
    if (!handlers[i]->is_preferred)
      continue;
    if (ArcIntentHelperBridge::IsIntentHelperPackage(
            handlers[i]->package_name)) {
      // If Chrome browser was selected as the preferred app, we shouldn't
      // create a throttle.
      DVLOG(1)
          << "Chrome browser is selected as the preferred app for this URL: "
          << url_for_logging;
    }
    return i;
  }
  return handlers.size();  // not found
}

}  // namespace

ArcNavigationThrottle::ArcNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    const ShowIntentPickerCallback& show_intent_picker_cb)
    : content::NavigationThrottle(navigation_handle),
      show_intent_picker_callback_(show_intent_picker_cb),
      ui_displayed_(false),
      weak_ptr_factory_(this) {}

ArcNavigationThrottle::~ArcNavigationThrottle() = default;

const char* ArcNavigationThrottle::GetNameForLogging() {
  return "ArcNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
ArcNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  starting_gurl_ = GetStartingGURL();
  Browser* browser =
      chrome::FindBrowserWithWebContents(navigation_handle()->GetWebContents());
  if (browser)
    chrome::SetIntentPickerViewVisibility(browser, false);
  return HandleRequest();
}

content::NavigationThrottle::ThrottleCheckResult
ArcNavigationThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(djacobo): Consider what to do when there is another url during the
  // same navigation that could be handled by ARC apps, two ideas are: 1) update
  // the bubble with a mix of both app candidates (if different) 2) show a
  // bubble based on the last url, thus closing all the previous ones.
  if (ui_displayed_)
    return content::NavigationThrottle::PROCEED;

  return HandleRequest();
}

content::NavigationThrottle::ThrottleCheckResult
ArcNavigationThrottle::HandleRequest() {
  DCHECK(!ui_displayed_);
  content::NavigationHandle* handle = navigation_handle();
  const GURL& url = handle->GetURL();

  // Always handle http(s) <form> submissions in Chrome for two reasons: 1) we
  // don't have a way to send POST data to ARC, and 2) intercepting http(s) form
  // submissions is not very important because such submissions are usually
  // done within the same domain. ShouldOverrideUrlLoading() below filters out
  // such submissions anyway.
  constexpr bool kAllowFormSubmit = false;

  // Ignore navigations with the CLIENT_REDIRECT qualifier on.
  constexpr bool kAllowClientRedirect = false;

  // We must never handle navigations started within a context menu.
  if (handle->WasStartedFromContextMenu())
    return content::NavigationThrottle::PROCEED;

  if (ShouldIgnoreNavigation(handle->GetPageTransition(), kAllowFormSubmit,
                             kAllowClientRedirect))
    return content::NavigationThrottle::PROCEED;

  if (!ShouldOverrideUrlLoading(starting_gurl_, url))
    return content::NavigationThrottle::PROCEED;

  ArcServiceManager* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return content::NavigationThrottle::PROCEED;

  auto* intent_helper_bridge = ArcIntentHelperBridge::GetForBrowserContext(
      handle->GetWebContents()->GetBrowserContext());
  if (!intent_helper_bridge)
    return content::NavigationThrottle::PROCEED;

  if (intent_helper_bridge->ShouldChromeHandleUrl(url)) {
    // Allow navigation to proceed if there isn't an android app that handles
    // the given URL.
    return content::NavigationThrottle::PROCEED;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance)
    return content::NavigationThrottle::PROCEED;

  // Assume the UI or a preferred app was found, reset to false only if we don't
  // find a valid app candidate.
  ui_displayed_ = true;
  instance->RequestUrlHandlerList(
      url.spec(), base::Bind(&ArcNavigationThrottle::OnAppCandidatesReceived,
                             weak_ptr_factory_.GetWeakPtr()));
  // We don't want to block the navigation, the only exception is here since we
  // need to know if we really need to launch the UI or not, navigation is
  // resumed right after we receive an answer from ARC's side (no user
  // interaction needed).
  return content::NavigationThrottle::DEFER;
}

GURL ArcNavigationThrottle::GetStartingGURL() const {
  // This helps us determine a reference GURL for the current NavigationHandle.
  // This is the order or preferrence: Referrer > LastCommittedURL > SiteURL,
  // GetSiteURL *should* only be used on very rare cases, e.g. when the
  // navigation goes from https: to http: on a new tab, thus losing the other
  // potential referrers.
  const GURL referrer_url = navigation_handle()->GetReferrer().url;
  if (referrer_url.is_valid() && !referrer_url.is_empty())
    return referrer_url;

  const GURL last_committed_url =
      navigation_handle()->GetWebContents()->GetLastCommittedURL();
  if (last_committed_url.is_valid() && !last_committed_url.is_empty())
    return last_committed_url;

  return navigation_handle()->GetStartingSiteInstance()->GetSiteURL();
}

// We received the array of app candidates to handle this URL (even the Chrome
// app is included).
void ArcNavigationThrottle::OnAppCandidatesReceived(
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::NavigationHandle* handle = navigation_handle();
  const GURL& url = handle->GetURL();

  if (!IsAppAvailable(handlers)) {
    // This scenario shouldn't be accessed as ArcNavigatinoThrottle is created
    // iff there are ARC apps which can actually handle the given URL.
    DVLOG(1) << "There are no app candidates for this URL: " << url;
    ui_displayed_ = false;
    Resume();
    return;
  }

  Resume();

  // If one of the apps is marked as preferred, use it right away without
  // showing the UI.
  const size_t index = FindPreferredApp(handlers, url);
  if (index != handlers.size()) {
    CloseReason close_reason = CloseReason::PREFERRED_ACTIVITY_FOUND;
    const std::string package_name = handlers[index]->package_name;

    // Make sure that the instance at least supports HandleUrl.
    auto* arc_service_manager = ArcServiceManager::Get();
    mojom::IntentHelperInstance* instance = nullptr;
    if (arc_service_manager) {
      instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrl);
    }

    if (!instance) {
      close_reason = CloseReason::ERROR;
    } else if (!ArcIntentHelperBridge::IsIntentHelperPackage(package_name)) {
      Browser* browser = chrome::FindBrowserWithWebContents(
          navigation_handle()->GetWebContents());
      if (browser)
        chrome::SetIntentPickerViewVisibility(browser, true);

      instance->HandleUrl(url.spec(), package_name);
    }

    Platform platform = GetDestinationPlatform(package_name, close_reason);
    RecordUma(close_reason, platform);
  } else {
    auto* intent_helper_bridge = ArcIntentHelperBridge::GetForBrowserContext(
        navigation_handle()->GetWebContents()->GetBrowserContext());
    if (!intent_helper_bridge) {
      LOG(ERROR) << "Cannot get an instance of ArcIntentHelperBridge";
      return;
    }
    std::vector<ArcIntentHelperBridge::ActivityName> activities;
    for (const auto& handler : handlers)
      activities.emplace_back(handler->package_name, handler->activity_name);

    intent_helper_bridge->GetActivityIcons(
        activities, base::Bind(&ArcNavigationThrottle::AsyncOnAppIconsReceived,
                               chrome::FindBrowserWithWebContents(
                                   navigation_handle()->GetWebContents()),
                               base::Passed(&handlers), url));
  }
}

// static
void ArcNavigationThrottle::AsyncOnAppIconsReceived(
    const Browser* browser,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
    const GURL& url,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  std::vector<AppInfo> app_info;

  for (const auto& handler : handlers) {
    gfx::Image icon;
    const arc::ArcIntentHelperBridge::ActivityName activity(
        handler->package_name, handler->activity_name);
    const auto it = icons->find(activity);

    app_info.emplace_back(it != icons->end() ? it->second.icon20 : gfx::Image(),
                          handler->package_name, handler->name);
  }

  chrome::QueryAndDisplayArcApps(
      browser, app_info,
      base::Bind(&ArcNavigationThrottle::AsyncOnIntentPickerClosed, url));
}

// static
void ArcNavigationThrottle::AsyncOnIntentPickerClosed(
    const GURL& url,
    const std::string& pkg,
    arc::ArcNavigationThrottle::CloseReason close_reason) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);
  }
  if (!instance)
    close_reason = CloseReason::ERROR;

  switch (close_reason) {
    case CloseReason::ERROR:
    case CloseReason::CHROME_PRESSED:
    case CloseReason::DIALOG_DEACTIVATED: {
      break;
    }
    case CloseReason::PREFERRED_ACTIVITY_FOUND: {
      if (!arc::ArcIntentHelperBridge::IsIntentHelperPackage(pkg))
        instance->HandleUrl(url.spec(), pkg);
      break;
    }
    case arc::ArcNavigationThrottle::CloseReason::ARC_APP_PREFERRED_PRESSED: {
      DCHECK(arc_service_manager);
      if (ARC_GET_INSTANCE_FOR_METHOD(
              arc_service_manager->arc_bridge_service()->intent_helper(),
              AddPreferredPackage)) {
        instance->AddPreferredPackage(pkg);
      }
      instance->HandleUrl(url.spec(), pkg);
      break;
    }
    case arc::ArcNavigationThrottle::CloseReason::CHROME_PREFERRED_PRESSED: {
      DCHECK(arc_service_manager);
      if (ARC_GET_INSTANCE_FOR_METHOD(
              arc_service_manager->arc_bridge_service()->intent_helper(),
              AddPreferredPackage)) {
        instance->AddPreferredPackage(pkg);
      }
      break;
    }
    case arc::ArcNavigationThrottle::CloseReason::ARC_APP_PRESSED: {
      instance->HandleUrl(url.spec(), pkg);
      break;
    }
    case arc::ArcNavigationThrottle::CloseReason::OBSOLETE_ALWAYS_PRESSED:
    case arc::ArcNavigationThrottle::CloseReason::OBSOLETE_JUST_ONCE_PRESSED:
    case arc::ArcNavigationThrottle::CloseReason::INVALID: {
      NOTREACHED();
      return;
    }
  }

  arc::ArcNavigationThrottle::Platform platform =
      arc::ArcNavigationThrottle::GetDestinationPlatform(pkg, close_reason);
  arc::ArcNavigationThrottle::RecordUma(close_reason, platform);
}

// static
size_t ArcNavigationThrottle::GetAppIndex(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    const std::string& selected_app_package) {
  for (size_t i = 0; i < handlers.size(); ++i) {
    if (handlers[i]->package_name == selected_app_package)
      return i;
  }
  return handlers.size();
}

// static
ArcNavigationThrottle::Platform ArcNavigationThrottle::GetDestinationPlatform(
    const std::string& selected_app_package,
    CloseReason close_reason) {
  return (close_reason != CloseReason::ERROR &&
          close_reason != CloseReason::DIALOG_DEACTIVATED &&
          !ArcIntentHelperBridge::IsIntentHelperPackage(selected_app_package))
             ? Platform::ARC
             : Platform::CHROME;
}

// static
void ArcNavigationThrottle::RecordUma(CloseReason close_reason,
                                      Platform platform) {
  UMA_HISTOGRAM_ENUMERATION("Arc.IntentHandlerAction",
                            static_cast<int>(close_reason),
                            static_cast<int>(CloseReason::SIZE));

  UMA_HISTOGRAM_ENUMERATION("Arc.IntentHandlerDestinationPlatform",
                            static_cast<int>(platform),
                            static_cast<int>(Platform::SIZE));
}

// static
bool ArcNavigationThrottle::IsAppAvailable(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return handlers.size() > 1 ||
         (handlers.size() == 1 && !ArcIntentHelperBridge::IsIntentHelperPackage(
                                      handlers[0]->package_name));
}

// static
bool ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
    const GURL& previous_url,
    const GURL& current_url) {
  return ShouldOverrideUrlLoading(previous_url, current_url);
}

// static
bool ArcNavigationThrottle::IsAppAvailableForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return IsAppAvailable(handlers);
}

// static
size_t ArcNavigationThrottle::FindPreferredAppForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return FindPreferredApp(handlers, GURL());
}

// static
bool ArcNavigationThrottle::IsSwapElementsNeeded(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    std::pair<size_t, size_t>* out_indices) {
  size_t chrome_app_index = 0;
  for (size_t i = 0; i < handlers.size(); ++i) {
    if (ArcIntentHelperBridge::IsIntentHelperPackage(
            handlers[i]->package_name)) {
      chrome_app_index = i;
      break;
    }
  }
  if (chrome_app_index < ArcNavigationThrottle::kMaxAppResults)
    return false;

  *out_indices = std::make_pair(ArcNavigationThrottle::kMaxAppResults - 1,
                                chrome_app_index);
  return true;
}

// static
void ArcNavigationThrottle::AsyncShowIntentPickerBubble(const Browser* browser,
                                                        const GURL& url) {
  arc::ArcServiceManager* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    DVLOG(1) << "Cannot get an instance of ArcServiceManager";
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance) {
    DVLOG(1) << "Cannot get access to RequestUrlHandlerList";
    return;
  }

  instance->RequestUrlHandlerList(
      url.spec(),
      base::Bind(&ArcNavigationThrottle::AsyncOnAppCandidatesReceived, browser,
                 url));
}

// static
void ArcNavigationThrottle::AsyncOnAppCandidatesReceived(
    const Browser* browser,
    const GURL& url,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers) {
  if (!IsAppAvailable(handlers)) {
    DVLOG(1) << "There are no app candidates for this URL";
    return;
  }

  auto* intent_helper_bridge = arc::ArcIntentHelperBridge::GetForBrowserContext(
      browser->tab_strip_model()->GetActiveWebContents()->GetBrowserContext());
  if (!intent_helper_bridge) {
    DVLOG(1) << "Cannot get an instance of ArcIntentHelperBridge";
    return;
  }

  std::vector<arc::ArcIntentHelperBridge::ActivityName> activities;
  for (const auto& handler : handlers)
    activities.emplace_back(handler->package_name, handler->activity_name);

  intent_helper_bridge->GetActivityIcons(
      activities, base::Bind(&ArcNavigationThrottle::AsyncOnAppIconsReceived,
                             browser, base::Passed(&handlers), url));
}

}  // namespace arc
