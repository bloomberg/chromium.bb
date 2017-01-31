// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/local_activity_resolver.h"
#include "components/arc/intent_helper/page_transition_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/page_transition_types.h"

namespace arc {

namespace {

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

  return !net::registry_controlled_domains::SameDomainOrHost(
      current_url, previous_url,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// Returns true if |handlers| contain one or more apps. When this function is
// called from OnAppCandidatesReceived, |handlers| always contain Chrome (aka
// intent_helper), but the function doesn't treat it as an app.
bool IsAppAvailable(const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return handlers.size() > 1 || (handlers.size() == 1 &&
                                 !ArcIntentHelperBridge::IsIntentHelperPackage(
                                     handlers[0]->package_name));
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
      previous_user_action_(CloseReason::INVALID),
      weak_ptr_factory_(this) {}

ArcNavigationThrottle::~ArcNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ArcNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  starting_gurl_ = GetStartingGURL();
  return HandleRequest();
}

content::NavigationThrottle::ThrottleCheckResult
ArcNavigationThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (previous_user_action_) {
    case CloseReason::ERROR:
    case CloseReason::DIALOG_DEACTIVATED:
      // User dismissed the dialog, or some error occurred before. Don't
      // repeatedly pop up the dialog.
      return content::NavigationThrottle::PROCEED;

    case CloseReason::ALWAYS_PRESSED:
    case CloseReason::JUST_ONCE_PRESSED:
    case CloseReason::PREFERRED_ACTIVITY_FOUND:
      // We must never show the intent picker for the same throttle more than
      // once and we must considerate that we may have redirections within the
      // same ArcNavigationThrottle even after seeing the UI and selecting an
      // app to handle the navigation. This section can be reached iff the user
      // selected Chrome to continue the navigation, since Resume() tells the
      // throttle to continue with the chain of redirections.
      //
      // For example, by clicking a youtube link on gmail you can see the
      // following URLs, assume our |starting_gurl_| is "http://www.google.com":
      //
      // 1) https://www.google.com/url?hl=en&q=https://youtube.com/watch?v=fake
      // 2) https://youtube.com/watch?v=fake
      // 3) https://www.youtube.com/watch?v=fake
      //
      // 1) was caught via WillStartRequest() and 2) and 3) are caught via
      // WillRedirectRequest().Step 2) triggers the intent picker and step 3)
      // will be seen iff the user picks Chrome, or if Chrome was marked as the
      // preferred app for this kind of URL. This happens since after choosing
      // Chrome we tell the throttle to Resume(), thus allowing for further
      // redirections.
      return content::NavigationThrottle::PROCEED;

    case CloseReason::INVALID:
      // No picker has previously been popped up for this - continue.
      break;
  }
  return HandleRequest();
}

content::NavigationThrottle::ThrottleCheckResult
ArcNavigationThrottle::HandleRequest() {
  const GURL& url = navigation_handle()->GetURL();

  // Always handle http(s) <form> submissions in Chrome for two reasons: 1) we
  // don't have a way to send POST data to ARC, and 2) intercepting http(s) form
  // submissions is not very important because such submissions are usually
  // done within the same domain. ShouldOverrideUrlLoading() below filters out
  // such submissions anyway.
  constexpr bool kAllowFormSubmit = false;

  // Ignore navigations with the CLIENT_REDIRECT qualifier on.
  constexpr bool kAllowClientRedirect = false;

  // We must never handle navigations started within a context menu.
  if (navigation_handle()->WasStartedFromContextMenu())
    return content::NavigationThrottle::PROCEED;

  if (ShouldIgnoreNavigation(navigation_handle()->GetPageTransition(),
                             kAllowFormSubmit, kAllowClientRedirect))
    return content::NavigationThrottle::PROCEED;

  if (!ShouldOverrideUrlLoading(starting_gurl_, url))
    return content::NavigationThrottle::PROCEED;

  ArcServiceManager* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return content::NavigationThrottle::PROCEED;

  scoped_refptr<LocalActivityResolver> local_resolver =
      arc_service_manager->activity_resolver();
  if (local_resolver->ShouldChromeHandleUrl(url)) {
    // Allow navigation to proceed if there isn't an android app that handles
    // the given URL.
    return content::NavigationThrottle::PROCEED;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance)
    return content::NavigationThrottle::PROCEED;
  instance->RequestUrlHandlerList(
      url.spec(), base::Bind(&ArcNavigationThrottle::OnAppCandidatesReceived,
                             weak_ptr_factory_.GetWeakPtr()));
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
  if (!IsAppAvailable(handlers)) {
    // This scenario shouldn't be accesed as ArcNavigationThrottle is created
    // iff there are ARC apps which can actually handle the given URL.
    DVLOG(1) << "There are no app candidates for this URL: "
             << navigation_handle()->GetURL();
    navigation_handle()->Resume();
    return;
  }

  // If one of the apps is marked as preferred, use it right away without
  // showing the UI.
  const size_t index =
      FindPreferredApp(handlers, navigation_handle()->GetURL());
  if (index != handlers.size()) {
    const std::string package_name = handlers[index]->package_name;
    OnIntentPickerClosed(std::move(handlers), package_name,
                         CloseReason::PREFERRED_ACTIVITY_FOUND);
    return;
  }

  std::pair<size_t, size_t> indices;
  if (IsSwapElementsNeeded(handlers, &indices))
    std::swap(handlers[indices.first], handlers[indices.second]);

  auto* intent_helper_bridge =
      ArcServiceManager::GetGlobalService<ArcIntentHelperBridge>();
  if (!intent_helper_bridge) {
    LOG(ERROR) << "Cannot get an instance of ArcIntentHelperBridge";
    navigation_handle()->Resume();
    return;
  }
  std::vector<ArcIntentHelperBridge::ActivityName> activities;
  for (const auto& handler : handlers)
    activities.emplace_back(handler->package_name, handler->activity_name);
  intent_helper_bridge->GetActivityIcons(
      activities,
      base::Bind(&ArcNavigationThrottle::OnAppIconsReceived,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&handlers)));
}

void ArcNavigationThrottle::OnAppIconsReceived(
    std::vector<mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<AppInfo> app_info;

  for (const auto& handler : handlers) {
    gfx::Image icon;
    const ArcIntentHelperBridge::ActivityName activity(handler->package_name,
                                                       handler->activity_name);
    const auto it = icons->find(activity);

    app_info.emplace_back(
        AppInfo(it != icons->end() ? it->second.icon20 : gfx::Image(),
                handler->package_name, handler->name));
  }

  show_intent_picker_callback_.Run(
      navigation_handle()->GetWebContents(), app_info,
      base::Bind(&ArcNavigationThrottle::OnIntentPickerClosed,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&handlers)));
}

void ArcNavigationThrottle::OnIntentPickerClosed(
    std::vector<mojom::IntentHandlerInfoPtr> handlers,
    const std::string& selected_app_package,
    CloseReason close_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const GURL& url = navigation_handle()->GetURL();
  content::NavigationHandle* handle = navigation_handle();
  previous_user_action_ = close_reason;

  // Make sure that the instance at least supports HandleUrl.
  auto* arc_service_manager = ArcServiceManager::Get();
  mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);
  }

  // Since we are selecting an app by its package name, we need to locate it
  // on the |handlers| structure before sending the IPC to ARC.
  const size_t selected_app_index = GetAppIndex(handlers, selected_app_package);
  if (!instance) {
    close_reason = CloseReason::ERROR;
  } else if (close_reason == CloseReason::JUST_ONCE_PRESSED ||
             close_reason == CloseReason::ALWAYS_PRESSED ||
             close_reason == CloseReason::PREFERRED_ACTIVITY_FOUND) {
    if (selected_app_index == handlers.size())
      close_reason = CloseReason::ERROR;
  }

  switch (close_reason) {
    case CloseReason::ERROR:
    case CloseReason::DIALOG_DEACTIVATED: {
      // If the user fails to select an option from the list, or the UI returned
      // an error or if |selected_app_index| is not a valid index, then resume
      // the navigation in Chrome.
      DVLOG(1) << "User didn't select a valid option, resuming navigation.";
      handle->Resume();
      break;
    }
    case CloseReason::ALWAYS_PRESSED: {
      // Call AddPreferredPackage if it is supported. Reusing the same
      // |instance| is okay.
      DCHECK(arc_service_manager);
      if (ARC_GET_INSTANCE_FOR_METHOD(
              arc_service_manager->arc_bridge_service()->intent_helper(),
              AddPreferredPackage)) {
        instance->AddPreferredPackage(
            handlers[selected_app_index]->package_name);
      }
      // fall through.
    }
    case CloseReason::JUST_ONCE_PRESSED:
    case CloseReason::PREFERRED_ACTIVITY_FOUND: {
      if (ArcIntentHelperBridge::IsIntentHelperPackage(
              handlers[selected_app_index]->package_name)) {
        handle->Resume();
      } else {
        instance->HandleUrl(url.spec(), selected_app_package);
        handle->CancelDeferredNavigation(
            content::NavigationThrottle::CANCEL_AND_IGNORE);
        if (handle->GetWebContents()->GetController().IsInitialNavigation())
          handle->GetWebContents()->Close();
      }
      break;
    }
    case CloseReason::INVALID: {
      NOTREACHED();
      return;
    }
  }

  Platform platform =
      GetDestinationPlatform(selected_app_package, close_reason);
  RecordUma(close_reason, platform);
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

}  // namespace arc
