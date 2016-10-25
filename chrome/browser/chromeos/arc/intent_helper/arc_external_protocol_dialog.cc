// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_external_protocol_dialog.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/browser/chromeos/external_protocol_dialog.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
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

constexpr uint32_t kMinVersionForHandleUrl = 2;
constexpr uint32_t kMinVersionForRequestUrlHandlerList = 2;
constexpr uint32_t kMinVersionForAddPreferredPackage = 7;

void RecordUma(ArcNavigationThrottle::CloseReason close_reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "Arc.IntentHandlerAction", static_cast<int>(close_reason),
      static_cast<int>(ArcNavigationThrottle::CloseReason::SIZE));
}

// Shows the Chrome OS' original external protocol dialog as a fallback.
void ShowFallbackExternalProtocolDialog(int render_process_host_id,
                                        int routing_id,
                                        const GURL& url) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  new ExternalProtocolDialog(web_contents, url);
}

scoped_refptr<ActivityIconLoader> GetIconLoader() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ArcServiceManager* arc_service_manager = ArcServiceManager::Get();
  return arc_service_manager ? arc_service_manager->icon_loader() : nullptr;
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
  auto* instance = ArcIntentHelperBridge::GetIntentHelperInstance(
      "HandleUrl", kMinVersionForHandleUrl);
  if (!instance)
    return;

  instance->HandleUrl(url_and_package.first.spec(), url_and_package.second);
  CloseTabIfNeeded(render_process_host_id, routing_id);
}

// A helper function called by GetAction().
GetActionResult GetActionInternal(
    const GURL& original_url,
    const mojom::IntentHandlerInfoPtr& handler,
    std::pair<GURL, std::string>* out_url_and_package) {
  if (!handler->fallback_url.is_null()) {
    *out_url_and_package = std::make_pair(GURL(handler->fallback_url.get()),
                                          handler->package_name.get());
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
    return GetActionResult::HANDLE_URL_IN_ARC;
  }

  // Unlike |handler->fallback_url|, the |original_url| should always be handled
  // in ARC since it's external to Chrome.
  *out_url_and_package =
      std::make_pair(original_url, handler->package_name.get());
  return GetActionResult::HANDLE_URL_IN_ARC;
}

// Gets an action that should be done when ARC has the |handlers| for the
// |original_url| and the user selects |selected_app_index|. When the user
// hasn't selected any app, |selected_app_index| must be set to
// |handlers.size()|.
//
// When the returned action is either OPEN_URL_IN_CHROME or HANDLE_URL_IN_ARC,
// |out_url_and_package| is filled accordingly.
GetActionResult GetAction(
    const GURL& original_url,
    const mojo::Array<mojom::IntentHandlerInfoPtr>& handlers,
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
      if (GetActionInternal(original_url, handlers[0], out_url_and_package) ==
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
      return GetActionInternal(original_url, handler, out_url_and_package);
    }
    // Ask the user to pick one.
    return GetActionResult::ASK_USER;
  }

  // The user has already made the selection. Decide how to open it, either in
  // Chrome or ARC.
  return GetActionInternal(original_url, handlers[selected_app_index],
                           out_url_and_package);
}

// Handles |url| if possible. Returns true if it is actually handled.
bool HandleUrl(int render_process_host_id,
               int routing_id,
               const GURL& url,
               const mojo::Array<mojom::IntentHandlerInfoPtr>& handlers,
               size_t selected_app_index,
               GetActionResult* out_result) {
  std::pair<GURL, std::string> url_and_package;
  const GetActionResult result =
      GetAction(url, handlers, selected_app_index, &url_and_package);
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

// Called when the dialog is closed.
void OnIntentPickerClosed(int render_process_host_id,
                          int routing_id,
                          const GURL& url,
                          mojo::Array<mojom::IntentHandlerInfoPtr> handlers,
                          const std::string& selected_app_package,
                          ArcNavigationThrottle::CloseReason close_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the user selected an app to continue the navigation, confirm that the
  // |package_name| matches a valid option and return the index.
  const size_t selected_app_index =
      ArcNavigationThrottle::GetAppIndex(handlers, selected_app_package);
  // Make sure that the instance at least supports HandleUrl.
  auto* instance = ArcIntentHelperBridge::GetIntentHelperInstance(
      "HandleUrl", kMinVersionForHandleUrl);
  if (!instance) {
    close_reason = ArcNavigationThrottle::CloseReason::ERROR;
  } else if (close_reason ==
                 ArcNavigationThrottle::CloseReason::JUST_ONCE_PRESSED ||
             close_reason ==
                 ArcNavigationThrottle::CloseReason::ALWAYS_PRESSED) {
    if (selected_app_index == handlers.size())
      close_reason = ArcNavigationThrottle::CloseReason::ERROR;
  }

  switch (close_reason) {
    case ArcNavigationThrottle::CloseReason::ALWAYS_PRESSED: {
      if (ArcIntentHelperBridge::GetIntentHelperInstance(
              "AddPreferredPackage", kMinVersionForAddPreferredPackage)) {
        instance->AddPreferredPackage(
            handlers[selected_app_index]->package_name);
      }
      // fall through.
    }
    case ArcNavigationThrottle::CloseReason::JUST_ONCE_PRESSED: {
      // Launch the selected app.
      HandleUrl(render_process_host_id, routing_id, url, handlers,
                selected_app_index, nullptr);
      break;
    }
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
      // The user didn't select any ARC activity. Show the Chrome OS dialog.
      ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id,
                                         url);
      break;
    }
  }
  RecordUma(close_reason);
}

// Called when ARC returned activity icons for the |handlers|.
void OnAppIconsReceived(
    int render_process_host_id,
    int routing_id,
    const GURL& url,
    mojo::Array<mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using AppInfo = arc::ArcNavigationThrottle::AppInfo;
  std::vector<AppInfo> app_info;

  for (const auto& handler : handlers) {
    const ActivityIconLoader::ActivityName activity(handler->package_name,
                                                    handler->activity_name);
    const auto it = icons->find(activity);
    app_info.emplace_back(
        AppInfo(it != icons->end() ? it->second.icon20 : gfx::Image(),
                handler->package_name, handler->name));
  }

  auto show_bubble_cb = base::Bind(ShowIntentPickerBubble());
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  show_bubble_cb.Run(web_contents, app_info,
                     base::Bind(OnIntentPickerClosed, render_process_host_id,
                                routing_id, url, base::Passed(&handlers)));
}

// Called when ARC returned a handler list for the |url|.
void OnUrlHandlerList(int render_process_host_id,
                      int routing_id,
                      const GURL& url,
                      mojo::Array<mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* instance = ArcIntentHelperBridge::GetIntentHelperInstance(
      "HandleUrl", kMinVersionForHandleUrl);
  scoped_refptr<ActivityIconLoader> icon_loader = GetIconLoader();

  if (!instance || !icon_loader) {
    // ARC is not running anymore. Show the Chrome OS dialog.
    ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id, url);
    return;
  }

  // Check if the |url| should be handled right away without showing the UI.
  GetActionResult result;
  if (HandleUrl(render_process_host_id, routing_id, url, handlers,
                handlers.size(), &result)) {
    if (result == GetActionResult::HANDLE_URL_IN_ARC)
      RecordUma(ArcNavigationThrottle::CloseReason::PREFERRED_ACTIVITY_FOUND);
    return;  // the |url| has been handled.
  }

  // Otherwise, retrieve icons of the activities.
  std::vector<ActivityIconLoader::ActivityName> activities;
  for (const auto& handler : handlers) {
    activities.emplace_back(handler->package_name, handler->activity_name);
  }
  icon_loader->GetActivityIcons(
      activities, base::Bind(OnAppIconsReceived, render_process_host_id,
                             routing_id, url, base::Passed(&handlers)));
}

}  // namespace

bool RunArcExternalProtocolDialog(const GURL& url,
                                  int render_process_host_id,
                                  int routing_id,
                                  ui::PageTransition page_transition,
                                  bool has_user_gesture) {
  // This function is for external protocols that Chrome cannot handle.
  DCHECK(!url.SchemeIsHTTPOrHTTPS()) << url;

  // Try to forward <form> submissions to ARC when possible.
  constexpr bool kAllowFormSubmit = true;
  if (ShouldIgnoreNavigation(page_transition, kAllowFormSubmit))
    return false;

  auto* instance = ArcIntentHelperBridge::GetIntentHelperInstance(
      "RequestUrlHandlerList", kMinVersionForRequestUrlHandlerList);
  if (!instance)
    return false;  // ARC is either not supported or not yet ready.

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents || !web_contents->GetBrowserContext() ||
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return false;
  }

  // Show ARC version of the dialog, which is IntentPickerBubbleView. To show
  // the bubble view, we need to ask ARC for a handler list first.
  instance->RequestUrlHandlerList(
      url.spec(),
      base::Bind(OnUrlHandlerList, render_process_host_id, routing_id, url));
  return true;
}

GetActionResult GetActionForTesting(
    const GURL& original_url,
    const mojo::Array<mojom::IntentHandlerInfoPtr>& handlers,
    size_t selected_app_index,
    std::pair<GURL, std::string>* out_url_and_package) {
  return GetAction(original_url, handlers, selected_app_index,
                   out_url_and_package);
}

}  // namespace arc
