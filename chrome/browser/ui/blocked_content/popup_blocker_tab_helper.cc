// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

const size_t kMaximumNumberOfPopups = 25;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PopupBlockerTabHelper);

struct PopupBlockerTabHelper::BlockedRequest {
  BlockedRequest(const chrome::NavigateParams& params,
                 const blink::mojom::WindowFeatures& window_features)
      : params(params), window_features(window_features) {}

  chrome::NavigateParams params;
  blink::mojom::WindowFeatures window_features;
};

PopupBlockerTabHelper::PopupBlockerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

PopupBlockerTabHelper::~PopupBlockerTabHelper() {
}

void PopupBlockerTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PopupBlockerTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PopupBlockerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Clear all page actions, blocked content notifications and browser actions
  // for this tab, unless this is an same-document navigation. Also only
  // consider main frame navigations that successfully committed.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Close blocked popups.
  if (!blocked_popups_.IsEmpty()) {
    blocked_popups_.Clear();
    PopupNotificationVisibilityChanged(false);
  }
}

void PopupBlockerTabHelper::PopupNotificationVisibilityChanged(
    bool visible) {
  if (!web_contents()->IsBeingDestroyed()) {
    TabSpecificContentSettings::FromWebContents(web_contents())->
        SetPopupsBlocked(visible);
  }
}

// static
bool PopupBlockerTabHelper::MaybeBlockPopup(
    content::WebContents* web_contents,
    const base::Optional<GURL>& opener_url,
    const chrome::NavigateParams& params,
    const content::OpenURLParams* open_url_params,
    const blink::mojom::WindowFeatures& window_features) {
  DCHECK(!open_url_params ||
         open_url_params->user_gesture == params.user_gesture);

  const bool user_gesture = params.user_gesture;
  if (!web_contents)
    return false;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePopupBlocking)) {
    return false;
  }

  auto* popup_blocker = PopupBlockerTabHelper::FromWebContents(web_contents);
  if (!popup_blocker)
    return false;

  // If an explicit opener is not given, use the current committed load in this
  // web contents. This is because A page can't spawn popups (or do anything
  // else, either) until its load commits, so when we reach here, the popup was
  // spawned by the NavigationController's last committed entry, not the active
  // entry.  For example, if a page opens a popup in an onunload() handler, then
  // the active entry is the page to be loaded as we navigate away from the
  // unloading page.
  const GURL& url =
      opener_url ? opener_url.value() : web_contents->GetLastCommittedURL();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (url.is_valid() &&
      HostContentSettingsMapFactory::GetForProfile(profile)->GetContentSetting(
          url, url, CONTENT_SETTINGS_TYPE_POPUPS, std::string()) ==
          CONTENT_SETTING_ALLOW) {
    return false;
  }

  // The subresource_filter triggers an extra aggressive popup blocker on
  // pages where ads are being blocked, even if there is a user gesture.
  if (user_gesture) {
    auto* driver_factory = subresource_filter::
        ContentSubresourceFilterDriverFactory::FromWebContents(web_contents);
    if (!driver_factory ||
        !driver_factory->throttle_manager()->ShouldDisallowNewWindow(
            open_url_params)) {
      return false;
    }
  }

  popup_blocker->AddBlockedPopup(params, window_features);
  return true;
}

void PopupBlockerTabHelper::AddBlockedPopup(
    const chrome::NavigateParams& params,
    const blink::mojom::WindowFeatures& window_features) {
  if (blocked_popups_.size() >= kMaximumNumberOfPopups)
    return;

  auto id = blocked_popups_.Add(
      base::MakeUnique<BlockedRequest>(params, window_features));
  TabSpecificContentSettings::FromWebContents(web_contents())->
      OnContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS);
  for (auto& observer : observers_)
    observer.BlockedPopupAdded(id, params.url);
}

void PopupBlockerTabHelper::ShowBlockedPopup(
    int32_t id,
    WindowOpenDisposition disposition) {
  BlockedRequest* popup = blocked_popups_.Lookup(id);
  if (!popup)
    return;

  // We set user_gesture to true here, so the new popup gets correctly focused.
  popup->params.user_gesture = true;
  if (disposition != WindowOpenDisposition::CURRENT_TAB)
    popup->params.disposition = disposition;

#if defined(OS_ANDROID)
  TabModelList::HandlePopupNavigation(&popup->params);
#else
  chrome::Navigate(&popup->params);
#endif
  if (popup->params.disposition == WindowOpenDisposition::NEW_POPUP &&
      popup->params.target_contents) {
    popup->params.target_contents->Send(new ChromeViewMsg_SetWindowFeatures(
        popup->params.target_contents->GetRenderViewHost()->GetRoutingID(),
        popup->window_features));
  }

  blocked_popups_.Remove(id);
  if (blocked_popups_.IsEmpty())
    PopupNotificationVisibilityChanged(false);
}

size_t PopupBlockerTabHelper::GetBlockedPopupsCount() const {
  return blocked_popups_.size();
}

PopupBlockerTabHelper::PopupIdMap
    PopupBlockerTabHelper::GetBlockedPopupRequests() {
  PopupIdMap result;
  for (base::IDMap<std::unique_ptr<BlockedRequest>>::const_iterator iter(
           &blocked_popups_);
       !iter.IsAtEnd(); iter.Advance()) {
    result[iter.GetCurrentKey()] = iter.GetCurrentValue()->params.url;
  }
  return result;
}
