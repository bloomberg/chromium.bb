// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_external_protocol_dialog.h"

#include <memory>
#include <string>
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
#include "content/public/browser/web_contents.h"
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
      instance->HandleUrl(url.spec(),
                          handlers[selected_app_index]->package_name);
      CloseTabIfNeeded(render_process_host_id, routing_id);
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

  if (!instance || !icon_loader || !handlers.size()) {
    // No handler is available on ARC side. Show the Chrome OS dialog.
    ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id, url);
    return;
  }

  // If one of the apps is marked as preferred, use it right away without
  // showing the UI. |is_preferred| will never be true unless the user
  // explicitly makes it the default with the "always" button.
  for (size_t i = 0; i < handlers.size(); ++i) {
    if (!handlers[i]->is_preferred)
      continue;
    instance->HandleUrl(url.spec(), handlers[i]->package_name);
    CloseTabIfNeeded(render_process_host_id, routing_id);
    RecordUma(ArcNavigationThrottle::CloseReason::PREFERRED_ACTIVITY_FOUND);
    return;
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

}  // namespace arc
