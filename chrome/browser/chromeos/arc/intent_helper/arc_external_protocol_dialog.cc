// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_external_protocol_dialog.h"

#include <memory>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/browser/chromeos/external_protocol_dialog.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/page_transition_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using content::WebContents;

namespace arc {

namespace {

// TODO(yusukes|djacobo): Find a better way to detect a request loop and remove
// the global variables.
base::LazyInstance<GURL>::DestructorAtExit g_last_url =
    LAZY_INSTANCE_INITIALIZER;
ui::PageTransition g_last_page_transition;

// Shows the Chrome OS' original external protocol dialog as a fallback.
void ShowFallbackExternalProtocolDialog(int render_process_host_id,
                                        int routing_id,
                                        const GURL& url) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  new ExternalProtocolDialog(web_contents, url);
}

void CloseTabIfNeeded(int render_process_host_id, int routing_id) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (web_contents && web_contents->GetController().IsInitialNavigation())
    web_contents->Close();
}

// Shows |url| in the current tab.
void OpenUrlInChrome(int render_process_host_id,
                     int routing_id,
                     const GURL& url) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents)
    return;

  // Use the PAGE_TRANSITION_FROM_API qualifier so that this nativation won't
  // end up showing the disambig dialog.
  const ui::PageTransition page_transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_API);
  constexpr bool kIsRendererInitiated = false;
  const content::OpenURLParams params(
      // TODO(yusukes): Send a non-empty referrer.
      url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      page_transition_type, kIsRendererInitiated);
  web_contents->OpenURL(params);
}

// Sends |url| to ARC.
void HandleUrlInArc(int render_process_host_id,
                    int routing_id,
                    const std::pair<GURL, std::string>& url_and_package) {
  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);
  if (!instance)
    return;

  instance->HandleUrl(url_and_package.first.spec(), url_and_package.second);
  CloseTabIfNeeded(render_process_host_id, routing_id);
}

// A helper function called by GetAction().
GetActionResult GetActionInternal(
    const GURL& original_url,
    bool always_ask_user,
    const mojom::IntentHandlerInfoPtr& handler,
    std::pair<GURL, std::string>* out_url_and_package) {
  if (handler->fallback_url.has_value()) {
    *out_url_and_package =
        std::make_pair(GURL(*handler->fallback_url), handler->package_name);
    if (ArcIntentHelperBridge::IsIntentHelperPackage(handler->package_name)) {
      // Since |package_name| is "Chrome", and |fallback_url| is not null, the
      // URL must be either http or https. Check it just in case, and if not,
      // fallback to HANDLE_URL_IN_ARC;
      if (out_url_and_package->first.SchemeIsHTTPOrHTTPS())
        return GetActionResult::OPEN_URL_IN_CHROME;

      LOG(WARNING) << "Failed to handle " << out_url_and_package->first
                   << " in Chrome. Falling back to ARC...";
    }
    // |fallback_url| which Chrome doesn't support is passed (e.g. market:).
    return always_ask_user ? GetActionResult::ASK_USER
                           : GetActionResult::HANDLE_URL_IN_ARC;
  }

  // Unlike |handler->fallback_url|, the |original_url| should always be handled
  // in ARC since it's external to Chrome.
  *out_url_and_package = std::make_pair(original_url, handler->package_name);
  return always_ask_user ? GetActionResult::ASK_USER
                         : GetActionResult::HANDLE_URL_IN_ARC;
}

// Gets an action that should be done when ARC has the |handlers| for the
// |original_url| and the user selects |selected_app_index|. When the user
// hasn't selected any app, |selected_app_index| must be set to
// |handlers.size()|. When |always_ask_user| is true, the function never
// returns HANDLE_URL_IN_ARC.
//
// When the returned action is either OPEN_URL_IN_CHROME or HANDLE_URL_IN_ARC,
// |out_url_and_package| is filled accordingly.
GetActionResult GetAction(
    const GURL& original_url,
    bool always_ask_user,
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    size_t selected_app_index,
    std::pair<GURL, std::string>* out_url_and_package) {
  DCHECK(out_url_and_package);
  if (!handlers.size())
    return GetActionResult::SHOW_CHROME_OS_DIALOG;  // no apps found.

  if (selected_app_index == handlers.size()) {
    // The user hasn't made the selection yet.

    // If |handlers| has only one element and its package is "Chrome", open
    // the fallback URL in the current tab without showing the dialog.
    if (handlers.size() == 1) {
      if (GetActionInternal(original_url, always_ask_user, handlers[0],
                            out_url_and_package) ==
          GetActionResult::OPEN_URL_IN_CHROME) {
        return GetActionResult::OPEN_URL_IN_CHROME;
      }
    }

    // If one of the apps is marked as preferred, use it right away without
    // showing the UI. |is_preferred| will never be true unless the user
    // explicitly makes it the default with the "always" button.
    for (size_t i = 0; i < handlers.size(); ++i) {
      const mojom::IntentHandlerInfoPtr& handler = handlers[i];
      if (!handler->is_preferred)
        continue;
      // A preferred activity is found. Decide how to open it, either in Chrome
      // or ARC.
      return GetActionInternal(original_url, always_ask_user, handler,
                               out_url_and_package);
    }
    // Ask the user to pick one.
    return GetActionResult::ASK_USER;
  }

  // The user has already made the selection. Decide how to open it, either in
  // Chrome or ARC.
  DCHECK(!always_ask_user)
      << "|always_ask_user| must be false when |selected_app_index| is valid.";
  return GetActionInternal(original_url, false, handlers[selected_app_index],
                           out_url_and_package);
}

// Handles |url| if possible. Returns true if it is actually handled.
bool HandleUrl(int render_process_host_id,
               int routing_id,
               const GURL& url,
               bool always_ask_user,
               const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
               size_t selected_app_index,
               GetActionResult* out_result) {
  std::pair<GURL, std::string> url_and_package;
  const GetActionResult result = GetAction(
      url, always_ask_user, handlers, selected_app_index, &url_and_package);
  if (out_result)
    *out_result = result;

  switch (result) {
    case GetActionResult::SHOW_CHROME_OS_DIALOG:
      ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id,
                                         url);
      return true;
    case GetActionResult::OPEN_URL_IN_CHROME:
      OpenUrlInChrome(render_process_host_id, routing_id,
                      url_and_package.first);
      return true;
    case GetActionResult::HANDLE_URL_IN_ARC:
      HandleUrlInArc(render_process_host_id, routing_id, url_and_package);
      return true;
    case GetActionResult::ASK_USER:
      break;
  }
  return false;
}

// Returns a fallback http(s) in |handlers| which Chrome can handle. Returns
// an empty GURL if none found.
GURL GetUrlToNavigateOnDeactivate(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  const GURL empty_url;
  for (size_t i = 0; i < handlers.size(); ++i) {
    std::pair<GURL, std::string> url_and_package;
    if (GetActionInternal(empty_url, false, handlers[i], &url_and_package) ==
        GetActionResult::OPEN_URL_IN_CHROME) {
      DCHECK(url_and_package.first.SchemeIsHTTPOrHTTPS());
      return url_and_package.first;
    }
  }
  return empty_url;  // nothing found.
}

// Called when the dialog is just deactivated without pressing one of the
// buttons.
void OnIntentPickerDialogDeactivated(
    int render_process_host_id,
    int routing_id,
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  const GURL url_to_open_in_chrome = GetUrlToNavigateOnDeactivate(handlers);
  if (url_to_open_in_chrome.is_empty())
    CloseTabIfNeeded(render_process_host_id, routing_id);
  else
    OpenUrlInChrome(render_process_host_id, routing_id, url_to_open_in_chrome);
}

// Called when the dialog is closed. Note that once we show the UI, we should
// never show the Chrome OS' fallback dialog.
void OnIntentPickerClosed(int render_process_host_id,
                          int routing_id,
                          const GURL& url,
                          std::vector<mojom::IntentHandlerInfoPtr> handlers,
                          const std::string& selected_app_package,
                          ArcNavigationThrottle::CloseReason close_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the user selected an app to continue the navigation, confirm that the
  // |package_name| matches a valid option and return the index.
  const size_t selected_app_index =
      ArcNavigationThrottle::GetAppIndex(handlers, selected_app_package);

  // Make sure that the instance at least supports HandleUrl.
  auto* arc_service_manager = ArcServiceManager::Get();
  mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);
  }

  if (!instance) {
    close_reason = ArcNavigationThrottle::CloseReason::ERROR;
  } else if (close_reason == ArcNavigationThrottle::CloseReason::
                                 OBSOLETE_JUST_ONCE_PRESSED ||
             close_reason ==
                 ArcNavigationThrottle::CloseReason::OBSOLETE_ALWAYS_PRESSED) {
    if (selected_app_index == handlers.size()) {
      close_reason = ArcNavigationThrottle::CloseReason::ERROR;
    } else {
      // The user has made a selection. Clear g_last_* variables.
      g_last_url.Get() = GURL();
      g_last_page_transition = ui::PageTransition();
    }
  }

  switch (close_reason) {
    case ArcNavigationThrottle::CloseReason::ARC_APP_PREFERRED_PRESSED: {
      DCHECK(arc_service_manager);
      if (ARC_GET_INSTANCE_FOR_METHOD(
              arc_service_manager->arc_bridge_service()->intent_helper(),
              AddPreferredPackage)) {
        instance->AddPreferredPackage(
            handlers[selected_app_index]->package_name);
      }
      // fall through.
    }
    case ArcNavigationThrottle::CloseReason::ARC_APP_PRESSED: {
      // Launch the selected app.
      HandleUrl(render_process_host_id, routing_id, url, false, handlers,
                selected_app_index, nullptr);
      break;
    }
    case ArcNavigationThrottle::CloseReason::CHROME_PREFERRED_PRESSED:
    case ArcNavigationThrottle::CloseReason::CHROME_PRESSED: {
      LOG(ERROR) << "Chrome is not a valid option for external protocol URLs";
      // fall through.
    }
    case ArcNavigationThrottle::CloseReason::OBSOLETE_ALWAYS_PRESSED:
    case ArcNavigationThrottle::CloseReason::OBSOLETE_JUST_ONCE_PRESSED:
    case ArcNavigationThrottle::CloseReason::PREFERRED_ACTIVITY_FOUND:
    case ArcNavigationThrottle::CloseReason::INVALID: {
      NOTREACHED();
      return;  // no UMA recording.
    }
    case ArcNavigationThrottle::CloseReason::ERROR: {
      LOG(ERROR) << "IntentPickerBubbleView returned CloseReason::ERROR: "
                 << "instance=" << instance
                 << ", selected_app_index=" << selected_app_index
                 << ", handlers.size=" << handlers.size();
      // fall through.
    }
    case ArcNavigationThrottle::CloseReason::DIALOG_DEACTIVATED: {
      // The user didn't select any ARC activity.
      OnIntentPickerDialogDeactivated(render_process_host_id, routing_id,
                                      handlers);
      break;
    }
  }

  ArcNavigationThrottle::Platform platform =
      ArcNavigationThrottle::GetDestinationPlatform(selected_app_package,
                                                    close_reason);
  ArcNavigationThrottle::RecordUma(close_reason, platform);
}

// Called when ARC returned activity icons for the |handlers|.
void OnAppIconsReceived(
    int render_process_host_id,
    int routing_id,
    const GURL& url,
    std::vector<mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using AppInfo = ArcNavigationThrottle::AppInfo;
  std::vector<AppInfo> app_info;

  for (const auto& handler : handlers) {
    const ArcIntentHelperBridge::ActivityName activity(handler->package_name,
                                                       handler->activity_name);
    const auto it = icons->find(activity);
    app_info.emplace_back(
        AppInfo(it != icons->end() ? it->second.icon20 : gfx::Image(),
                handler->package_name, handler->name));
  }

  auto show_bubble_cb = base::Bind(ShowIntentPickerBubble());
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  show_bubble_cb.Run(nullptr /* anchor_view */, web_contents, app_info,
                     base::Bind(OnIntentPickerClosed, render_process_host_id,
                                routing_id, url, base::Passed(&handlers)));
}

// Called when ARC returned a handler list for the |url|.
void OnUrlHandlerList(int render_process_host_id,
                      int routing_id,
                      const GURL& url,
                      bool always_ask_user,
                      std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager) {
    // ARC is not running anymore. Show the Chrome OS dialog.
    ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id, url);
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  auto* intent_helper_bridge =
      web_contents ? ArcIntentHelperBridge::GetForBrowserContext(
                         web_contents->GetBrowserContext())
                   : nullptr;
  if (!instance || !intent_helper_bridge) {
    // ARC is not running anymore. Show the Chrome OS dialog.
    ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id, url);
    return;
  }

  // Check if the |url| should be handled right away without showing the UI.
  GetActionResult result;
  if (HandleUrl(render_process_host_id, routing_id, url, always_ask_user,
                handlers, handlers.size(), &result)) {
    if (result == GetActionResult::HANDLE_URL_IN_ARC) {
      ArcNavigationThrottle::RecordUma(
          ArcNavigationThrottle::CloseReason::PREFERRED_ACTIVITY_FOUND,
          ArcNavigationThrottle::Platform::ARC);
    }
    return;  // the |url| has been handled.
  }

  // Otherwise, retrieve icons of the activities. First, swap |handler| elements
  // to ensure Chrome is visible in the UI by default. Since this function is
  // for handling external protocols, Chrome is rarely in the list, but if the
  // |url| is intent: with fallback or geo:, for example, it may be.
  std::pair<size_t, size_t> indices;
  if (ArcNavigationThrottle::IsSwapElementsNeeded(handlers, &indices))
    std::swap(handlers[indices.first], handlers[indices.second]);

  // Then request the icons.
  std::vector<ArcIntentHelperBridge::ActivityName> activities;
  for (const auto& handler : handlers) {
    activities.emplace_back(handler->package_name, handler->activity_name);
  }
  intent_helper_bridge->GetActivityIcons(
      activities, base::Bind(OnAppIconsReceived, render_process_host_id,
                             routing_id, url, base::Passed(&handlers)));
}

// Returns true if the |url| is safe to be forwarded to ARC without showing the
// disambig dialog when there is a preferred app on ARC for the |url|. Note that
// this function almost always returns true (i.e. "safe") except for very rare
// situations mentioned below.
// TODO(yusukes|djacobo): Find a better way to detect a request loop and remove
// these heuristics.
bool IsSafeToRedirectToArcWithoutUserConfirmation(
    const GURL& url,
    ui::PageTransition page_transition,
    const GURL& last_url,
    ui::PageTransition last_page_transition) {
  // Return "safe" unless both transition flags are FROM_API because the only
  // unsafe situation we know is infinite tab creation loop with FROM_API
  // (b/30125340).
  if (!(page_transition & ui::PAGE_TRANSITION_FROM_API) ||
      !(last_page_transition & ui::PAGE_TRANSITION_FROM_API)) {
    return true;
  }

  // Return "safe" unless both URLs are for the same app.
  return url.scheme() != last_url.scheme();
}

}  // namespace

bool RunArcExternalProtocolDialog(const GURL& url,
                                  int render_process_host_id,
                                  int routing_id,
                                  ui::PageTransition page_transition,
                                  bool has_user_gesture) {
  // This function is for external protocols that Chrome cannot handle.
  DCHECK(!url.SchemeIsHTTPOrHTTPS()) << url;

  const bool always_ask_user = !IsSafeToRedirectToArcWithoutUserConfirmation(
      url, page_transition, g_last_url.Get(), g_last_page_transition);
  LOG_IF(WARNING, always_ask_user)
      << "RunArcExternalProtocolDialog: repeatedly handling external protocol "
      << "redirection to " << url
      << " started from API: last_url=" << g_last_url.Get();

  // This function is called only on the UI thread. Updating g_last_* variables
  // without a lock is safe.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_last_url.Get() = url;
  g_last_page_transition = page_transition;

  // For external protocol navigation, always ignore the FROM_API qualifier.
  // We sometimes do need to forward a request with FROM_API to ARC, or
  // AppAuth may not work (b/33208965). This is safe as long as we properly
  // use |always_ask_user|.
  const ui::PageTransition masked_page_transition =
      MaskOutPageTransition(page_transition, ui::PAGE_TRANSITION_FROM_API);

  if (ShouldIgnoreNavigation(masked_page_transition,
                             true /* allow_form_submit */,
                             true /* allow_client_redirect */)) {
    LOG(WARNING) << "RunArcExternalProtocolDialog: ignoring " << url
                 << " with PageTransition=" << masked_page_transition;
    return false;
  }

  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;  // ARC is either not supported or not yet ready.

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance)
    return false;  // the same.

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents || !web_contents->GetBrowserContext() ||
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return false;
  }

  // Show ARC version of the dialog, which is IntentPickerBubbleView. To show
  // the bubble view, we need to ask ARC for a handler list first.
  instance->RequestUrlHandlerList(
      url.spec(), base::Bind(OnUrlHandlerList, render_process_host_id,
                             routing_id, url, always_ask_user));
  return true;
}

GetActionResult GetActionForTesting(
    const GURL& original_url,
    bool always_ask_user,
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    size_t selected_app_index,
    std::pair<GURL, std::string>* out_url_and_package) {
  return GetAction(original_url, always_ask_user, handlers, selected_app_index,
                   out_url_and_package);
}

GURL GetUrlToNavigateOnDeactivateForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return GetUrlToNavigateOnDeactivate(handlers);
}

bool IsSafeToRedirectToArcWithoutUserConfirmationForTesting(
    const GURL& url,
    ui::PageTransition page_transition,
    const GURL& last_url,
    ui::PageTransition last_page_transition) {
  return IsSafeToRedirectToArcWithoutUserConfirmation(
      url, page_transition, last_url, last_page_transition);
}

}  // namespace arc
