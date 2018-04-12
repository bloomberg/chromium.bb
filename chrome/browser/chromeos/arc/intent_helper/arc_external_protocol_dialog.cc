// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_external_protocol_dialog.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/browser/chromeos/external_protocol_dialog.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
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

// static
const char ArcWebContentsData::kArcTransitionFlag[] = "ArcTransition";

namespace {

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

// Tells whether or not Chrome is an app candidate for the current navigation.
bool IsChromeAnAppCandidate(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  for (const auto& handle : handlers) {
    if (ArcIntentHelperBridge::IsIntentHelperPackage(handle->package_name))
      return true;
  }
  return false;
}

// Shows |url| in the current tab.
void OpenUrlInChrome(int render_process_host_id,
                     int routing_id,
                     const GURL& url) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents)
    return;

  // TODO(djacobo): Decide whether or not we should propagate FROM_API here.
  const ui::PageTransition page_transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_API);
  constexpr bool kIsRendererInitiated = false;
  const content::OpenURLParams params(
      // TODO(djacobo): Send a non-empty referrer.
      url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      page_transition_type, kIsRendererInitiated);
  web_contents->OpenURL(params);
}

mojom::IntentInfoPtr CreateIntentInfo(const GURL& url, bool ui_bypassed) {
  // Create an intent with action VIEW, the |url| we are redirecting the user to
  // and a flag that tells whether or not the user interacted with the picker UI
  arc::mojom::IntentInfoPtr intent = arc::mojom::IntentInfo::New();
  intent->action = "org.chromium.arc.intent.action.VIEW";
  intent->data = url.spec();
  intent->ui_bypassed = ui_bypassed;

  return intent;
}

// Sends |url| to ARC.
void HandleUrlInArc(int render_process_host_id,
                    int routing_id,
                    const std::pair<GURL, ArcIntentHelperBridge::ActivityName>&
                        url_and_activity,
                    bool ui_bypassed) {
  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleIntent);
  if (!instance)
    return;

  // We want to inform ARC about whether or not the user interacted with the
  // picker UI, also since we want to be more explicit about the package and
  // activity we are using, we are relying in HandleIntent() to comunicate back
  // to ARC.
  arc::mojom::ActivityNamePtr activity = arc::mojom::ActivityName::New();
  activity->package_name = url_and_activity.second.package_name;
  activity->activity_name = url_and_activity.second.activity_name;

  instance->HandleIntent(CreateIntentInfo(url_and_activity.first, ui_bypassed),
                         std::move(activity));
  CloseTabIfNeeded(render_process_host_id, routing_id);
}

// A helper function called by GetAction().
GetActionResult GetActionInternal(
    const GURL& original_url,
    const mojom::IntentHandlerInfoPtr& handler,
    std::pair<GURL, ArcIntentHelperBridge::ActivityName>*
        out_url_and_activity_name) {
  if (handler->fallback_url.has_value()) {
    *out_url_and_activity_name =
        std::make_pair(GURL(*handler->fallback_url),
                       ArcIntentHelperBridge::ActivityName(
                           handler->package_name, handler->activity_name));
    if (ArcIntentHelperBridge::IsIntentHelperPackage(handler->package_name)) {
      // Since |package_name| is "Chrome", and |fallback_url| is not null, the
      // URL must be either http or https. Check it just in case, and if not,
      // fallback to HANDLE_URL_IN_ARC;
      if (out_url_and_activity_name->first.SchemeIsHTTPOrHTTPS())
        return GetActionResult::OPEN_URL_IN_CHROME;

      LOG(WARNING) << "Failed to handle " << out_url_and_activity_name->first
                   << " in Chrome. Falling back to ARC...";
    }
    // |fallback_url| which Chrome doesn't support is passed (e.g. market:).
    return GetActionResult::HANDLE_URL_IN_ARC;
  }

  // Unlike |handler->fallback_url|, the |original_url| should always be handled
  // in ARC since it's external to Chrome.
  *out_url_and_activity_name = std::make_pair(
      original_url, ArcIntentHelperBridge::ActivityName(
                        handler->package_name, handler->activity_name));
  return GetActionResult::HANDLE_URL_IN_ARC;
}

// Gets an action that should be done when ARC has the |handlers| for the
// |original_url| and the user selects |selected_app_index|. When the user
// hasn't selected any app, |selected_app_index| must be set to
// |handlers.size()|.
//
// When the returned action is either OPEN_URL_IN_CHROME or HANDLE_URL_IN_ARC,
// |out_url_and_activity_name| is filled accordingly.
//
// |in_out_safe_to_bypass_ui| is used to reflect whether or not we should
// display the UI: it initially informs whether or not this navigation was
// initiated within ARC, and then gets double-checked and used to store whether
// or not the user can safely bypass the UI.
GetActionResult GetAction(
    const GURL& original_url,
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    size_t selected_app_index,
    std::pair<GURL, ArcIntentHelperBridge::ActivityName>*
        out_url_and_activity_name,
    bool* in_out_safe_to_bypass_ui) {
  DCHECK(out_url_and_activity_name);
  if (!handlers.size()) {
    *in_out_safe_to_bypass_ui = false;
    return GetActionResult::SHOW_CHROME_OS_DIALOG;  // no apps found.
  }

  if (selected_app_index == handlers.size()) {
    // The user hasn't made the selection yet.

    // If |handlers| has only one element and either of the following conditions
    // is met, open the URL in Chrome or ARC without showing the picker UI.
    // 1) its package is "Chrome", open the fallback URL in the current tab
    // without showing the dialog.
    // 2) its package is not "Chrome" but it has been marked as
    // |in_out_safe_to_bypass_ui|, this means that we trust the current tab
    // since its content was originated from ARC.
    if (handlers.size() == 1) {
      const GetActionResult internal_result = GetActionInternal(
          original_url, handlers[0], out_url_and_activity_name);

      if ((internal_result == GetActionResult::HANDLE_URL_IN_ARC &&
           *in_out_safe_to_bypass_ui) ||
          internal_result == GetActionResult::OPEN_URL_IN_CHROME) {
        // Make sure the |in_out_safe_to_bypass_ui| flag is actually marked, its
        // maybe not important for OPEN_URL_IN_CHROME but just for consistency.
        *in_out_safe_to_bypass_ui = true;
        return internal_result;
      }
    }

    // Since we have 2+ app candidates we should display the UI, unless there is
    // an already preferred app. |is_preferred| will never be true unless the
    // user explicitly marked it as such.
    *in_out_safe_to_bypass_ui = false;
    for (size_t i = 0; i < handlers.size(); ++i) {
      const mojom::IntentHandlerInfoPtr& handler = handlers[i];
      if (!handler->is_preferred)
        continue;
      // This is another way to bypass the UI, since the user already expressed
      // some sort of preference.
      *in_out_safe_to_bypass_ui = true;
      // A preferred activity is found. Decide how to open it, either in Chrome
      // or ARC.
      return GetActionInternal(original_url, handler,
                               out_url_and_activity_name);
    }
    // Ask the user to pick one.
    return GetActionResult::ASK_USER;
  }

  // The user already made a selection so this should be false.
  *in_out_safe_to_bypass_ui = false;
  return GetActionInternal(original_url, handlers[selected_app_index],
                           out_url_and_activity_name);
}

// Returns true if the |url| is safe to be forwarded to ARC without showing the
// disambig dialog, besides having this flag set we need to check that there is
// only one app candidate, this is enforced via GetAction(). Any navigation
// coming from ARC via ChromeShellDelegate MUST be marked as such.
//
// Mark as not "safe" (aka return false) on the contrary, most likely those
// cases will require the user to pass thru the intent picker UI.
bool IsSafeToRedirectToArcWithoutUserConfirmation(content::WebContents* tab) {
  const char* key =
      arc::ArcWebContentsData::ArcWebContentsData::kArcTransitionFlag;
  arc::ArcWebContentsData* arc_data =
      static_cast<arc::ArcWebContentsData*>(tab->GetUserData(key));
  if (!arc_data)
    return false;

  tab->RemoveUserData(key);
  return true;
}

// Handles |url| if possible. Returns true if it is actually handled.
bool HandleUrl(int render_process_host_id,
               int routing_id,
               const GURL& url,
               const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
               size_t selected_app_index,
               GetActionResult* out_result) {
  auto url_and_activity_name = std::make_pair(
      GURL(),
      ArcIntentHelperBridge::ActivityName{"" /* package */, "" /* activity */});

  // TODO(djacobo): Evaluate if passing around WebContents instead of
  // |render_process_host_id| and |rounting_id| would be equivalent.
  WebContents* tab =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  DCHECK(tab);

  bool safe_to_bypass_ui = IsSafeToRedirectToArcWithoutUserConfirmation(tab);

  const GetActionResult result =
      GetAction(url, handlers, selected_app_index, &url_and_activity_name,
                &safe_to_bypass_ui);
  if (out_result)
    *out_result = result;

  switch (result) {
    case GetActionResult::SHOW_CHROME_OS_DIALOG:
      ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id,
                                         url);
      return true;
    case GetActionResult::OPEN_URL_IN_CHROME:
      OpenUrlInChrome(render_process_host_id, routing_id,
                      url_and_activity_name.first);
      return true;
    case GetActionResult::HANDLE_URL_IN_ARC:
      HandleUrlInArc(render_process_host_id, routing_id, url_and_activity_name,
                     safe_to_bypass_ui);
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
    auto url_and_package =
        std::make_pair(GURL(), ArcIntentHelperBridge::ActivityName{
                                   "" /* package */, "" /* activity */});
    if (GetActionInternal(empty_url, handlers[i], &url_and_package) ==
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
                          chromeos::AppType app_type,
                          chromeos::IntentPickerCloseReason reason,
                          bool should_persist) {
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

  if (!instance)
    reason = chromeos::IntentPickerCloseReason::ERROR;

  if (reason == chromeos::IntentPickerCloseReason::OPEN_APP ||
      reason == chromeos::IntentPickerCloseReason::STAY_IN_CHROME) {
    if (selected_app_index == handlers.size()) {
      reason = chromeos::IntentPickerCloseReason::ERROR;
    }
  }

  switch (reason) {
    case chromeos::IntentPickerCloseReason::OPEN_APP:
      // Only ARC apps are offered in the external protocol intent picker, so if
      // the user decided to open in app the type must be ARC.
      DCHECK_EQ(chromeos::AppType::ARC, app_type);
      DCHECK(arc_service_manager);

      if (should_persist) {
        if (ARC_GET_INSTANCE_FOR_METHOD(
                arc_service_manager->arc_bridge_service()->intent_helper(),
                AddPreferredPackage)) {
          instance->AddPreferredPackage(
              handlers[selected_app_index]->package_name);
        }
      }

      // Launch the selected app.
      HandleUrl(render_process_host_id, routing_id, url, handlers,
                selected_app_index, nullptr);
      break;
    case chromeos::IntentPickerCloseReason::PREFERRED_APP_FOUND:
      // We shouldn't be here if a preferred app was found.
      NOTREACHED();
      return;  // no UMA recording.
    case chromeos::IntentPickerCloseReason::STAY_IN_CHROME:
      LOG(ERROR) << "Chrome is not a valid option for external protocol URLs";
      NOTREACHED();
      return;  // no UMA recording.
    case chromeos::IntentPickerCloseReason::ERROR:
      LOG(ERROR) << "IntentPickerBubbleView returned CloseReason::ERROR: "
                 << "instance=" << instance
                 << ", selected_app_index=" << selected_app_index
                 << ", handlers.size=" << handlers.size();
      FALLTHROUGH;
    case chromeos::IntentPickerCloseReason::DIALOG_DEACTIVATED:
      // The user didn't select any ARC activity.
      OnIntentPickerDialogDeactivated(render_process_host_id, routing_id,
                                      handlers);
      break;
  }

  ArcNavigationThrottle::RecordUma(selected_app_package, app_type, reason,
                                   should_persist);
}

// Called when ARC returned activity icons for the |handlers|.
void OnAppIconsReceived(
    int render_process_host_id,
    int routing_id,
    const GURL& url,
    std::vector<mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using AppInfo = chromeos::IntentPickerAppInfo;
  std::vector<AppInfo> app_info;

  for (const auto& handler : handlers) {
    const ArcIntentHelperBridge::ActivityName activity(handler->package_name,
                                                       handler->activity_name);
    const auto it = icons->find(activity);
    app_info.emplace_back(chromeos::AppType::ARC,
                          it != icons->end() ? it->second.icon16 : gfx::Image(),
                          handler->package_name, handler->name);
  }

  auto show_bubble_cb = base::Bind(ShowIntentPickerBubble());
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  show_bubble_cb.Run(nullptr /* anchor_view */, web_contents,
                     std::move(app_info), !IsChromeAnAppCandidate(handlers),
                     base::Bind(OnIntentPickerClosed, render_process_host_id,
                                routing_id, url, base::Passed(&handlers)));
}

// Called when ARC returned a handler list for the |url|.
void OnUrlHandlerList(int render_process_host_id,
                      int routing_id,
                      const GURL& url,
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
  if (HandleUrl(render_process_host_id, routing_id, url, handlers,
                handlers.size(), &result)) {
    if (result == GetActionResult::HANDLE_URL_IN_ARC) {
      ArcNavigationThrottle::RecordUma(
          std::string(), chromeos::AppType::ARC,
          chromeos::IntentPickerCloseReason::PREFERRED_APP_FOUND,
          false /* should_persist */);
    }
    return;  // the |url| has been handled.
  }

  // Otherwise, retrieve icons of the activities. Since this function is for
  // handling external protocols, Chrome is rarely in the list, but if the |url|
  // is intent: with fallback or geo:, for example, it may be.
  std::vector<ArcIntentHelperBridge::ActivityName> activities;
  for (const auto& handler : handlers) {
    activities.emplace_back(handler->package_name, handler->activity_name);
  }
  intent_helper_bridge->GetActivityIcons(
      activities, base::BindOnce(OnAppIconsReceived, render_process_host_id,
                                 routing_id, url, std::move(handlers)));
}

}  // namespace

bool RunArcExternalProtocolDialog(const GURL& url,
                                  int render_process_host_id,
                                  int routing_id,
                                  ui::PageTransition page_transition,
                                  bool has_user_gesture) {
  // This function is for external protocols that Chrome cannot handle.
  DCHECK(!url.SchemeIsHTTPOrHTTPS()) << url;

  // For external protocol navigation, always ignore the FROM_API qualifier.
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
      url.spec(), base::BindOnce(OnUrlHandlerList, render_process_host_id,
                                 routing_id, url));
  return true;
}

GetActionResult GetActionForTesting(
    const GURL& original_url,
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers,
    size_t selected_app_index,
    std::pair<GURL, ArcIntentHelperBridge::ActivityName>*
        out_url_and_activity_name,
    bool* safe_to_bypass_ui) {
  return GetAction(original_url, handlers, selected_app_index,
                   out_url_and_activity_name, safe_to_bypass_ui);
}

GURL GetUrlToNavigateOnDeactivateForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return GetUrlToNavigateOnDeactivate(handlers);
}

bool IsSafeToRedirectToArcWithoutUserConfirmationForTesting(
    content::WebContents* tab) {
  return IsSafeToRedirectToArcWithoutUserConfirmation(tab);
}

bool IsChromeAnAppCandidateForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& handlers) {
  return IsChromeAnAppCandidate(handlers);
}

}  // namespace arc
