// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_external_protocol_dialog.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/arc/arc_navigation_throttle.h"
#include "chrome/browser/chromeos/arc/page_transition_util.h"
#include "chrome/browser/chromeos/external_protocol_dialog.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using content::WebContents;

namespace arc {

namespace {

constexpr uint32_t kMinInstanceVersion = 3;  // RequestActivityIcons' MinVersion

// Shows the Chrome OS' original external protocol dialog as a fallback.
void ShowFallbackExternalProtocolDialog(int render_process_host_id,
                                        int routing_id,
                                        const GURL& url) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  new ExternalProtocolDialog(web_contents, url);
}

mojom::IntentHelperInstance* GetIntentHelper() {
  return ArcIntentHelperBridge::GetIntentHelperInstance(kMinInstanceVersion);
}

scoped_refptr<ActivityIconLoader> GetIconLoader() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ArcServiceManager* arc_service_manager = ArcServiceManager::Get();
  return arc_service_manager ? arc_service_manager->icon_loader() : nullptr;
}

// Called when the dialog is closed.
void OnIntentPickerClosed(int render_process_host_id,
                          int routing_id,
                          const GURL& url,
                          mojo::Array<mojom::UrlHandlerInfoPtr> handlers,
                          size_t selected_app_index,
                          ArcNavigationThrottle::CloseReason close_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::IntentHelperInstance* intent_helper = GetIntentHelper();
  if (!intent_helper || selected_app_index >= handlers.size())
    close_reason = ArcNavigationThrottle::CloseReason::ERROR;

  switch (close_reason) {
    case ArcNavigationThrottle::CloseReason::ALWAYS_PRESSED: {
      intent_helper->AddPreferredPackage(
          handlers[selected_app_index]->package_name);
      // fall through.
    }
    case ArcNavigationThrottle::CloseReason::JUST_ONCE_PRESSED:
    case ArcNavigationThrottle::CloseReason::PREFERRED_ACTIVITY_FOUND: {
      // Launch the selected app.
      intent_helper->HandleUrl(url.spec(),
                               handlers[selected_app_index]->package_name);
      break;
    }
    case ArcNavigationThrottle::CloseReason::ERROR:
    case ArcNavigationThrottle::CloseReason::INVALID: {
      LOG(ERROR) << "IntentPickerBubbleView returned unexpected close_reason: "
                 << static_cast<int>(close_reason);
      // fall through.
    }
    case ArcNavigationThrottle::CloseReason::DIALOG_DEACTIVATED: {
      // The user didn't select any ARC activity. Show the Chrome OS dialog.
      ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id,
                                         url);
      break;
    }
  }
}

// Called when ARC returned activity icons for the |handlers|.
void OnAppIconsReceived(
    int render_process_host_id,
    int routing_id,
    const GURL& url,
    mojo::Array<mojom::UrlHandlerInfoPtr> handlers,
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using NameAndIcon = std::pair<std::string, gfx::Image>;
  std::vector<NameAndIcon> app_info;

  for (const auto& handler : handlers) {
    const ActivityIconLoader::ActivityName activity(handler->package_name,
                                                    handler->activity_name);
    const auto it = icons->find(activity);
    app_info.emplace_back(
        handler->name, it != icons->end() ? it->second.icon20 : gfx::Image());
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
                      mojo::Array<mojom::UrlHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::IntentHelperInstance* intent_helper = GetIntentHelper();
  scoped_refptr<ActivityIconLoader> icon_loader = GetIconLoader();

  if (!intent_helper || !icon_loader || !handlers.size()) {
    // No handler is available on ARC side. Show the Chrome OS dialog.
    ShowFallbackExternalProtocolDialog(render_process_host_id, routing_id, url);
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
  if (ShouldIgnoreNavigation(page_transition))
    return false;

  mojom::IntentHelperInstance* intent_helper = GetIntentHelper();
  if (!intent_helper)
    return false;  // ARC is either not supported or not yet ready.

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents || !web_contents->GetBrowserContext() ||
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return false;
  }

  // Show ARC version of the dialog, which is IntentPickerBubbleView. To show
  // the bubble view, we need to ask ARC for a handler list first.
  intent_helper->RequestUrlHandlerList(
      url.spec(),
      base::Bind(OnUrlHandlerList, render_process_host_id, routing_id, url));
  return true;
}

}  // namespace arc
