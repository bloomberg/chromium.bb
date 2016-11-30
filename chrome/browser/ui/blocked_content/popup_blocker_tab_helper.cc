// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/features.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

using blink::WebWindowFeatures;

const size_t kMaximumNumberOfPopups = 25;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PopupBlockerTabHelper);

struct PopupBlockerTabHelper::BlockedRequest {
  BlockedRequest(const chrome::NavigateParams& params,
                 const WebWindowFeatures& window_features)
      : params(params), window_features(window_features) {}

  chrome::NavigateParams params;
  WebWindowFeatures window_features;
};

PopupBlockerTabHelper::PopupBlockerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

PopupBlockerTabHelper::~PopupBlockerTabHelper() {
}

void PopupBlockerTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Clear all page actions, blocked content notifications and browser actions
  // for this tab, unless this is an in-page navigation.
  if (details.is_in_page)
    return;

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

bool PopupBlockerTabHelper::MaybeBlockPopup(
    const chrome::NavigateParams& params,
    const WebWindowFeatures& window_features) {
  // A page can't spawn popups (or do anything else, either) until its load
  // commits, so when we reach here, the popup was spawned by the
  // NavigationController's last committed entry, not the active entry.  For
  // example, if a page opens a popup in an onunload() handler, then the active
  // entry is the page to be loaded as we navigate away from the unloading
  // page.  For this reason, we can't use GetURL() to get the opener URL,
  // because it returns the active entry.
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  GURL creator = entry ? entry->GetVirtualURL() : GURL();
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  if (creator.is_valid() &&
      HostContentSettingsMapFactory::GetForProfile(profile)->GetContentSetting(
          creator, creator, CONTENT_SETTINGS_TYPE_POPUPS, std::string()) ==
          CONTENT_SETTING_ALLOW) {
    return false;
  }

  AddBlockedPopup(params, window_features);
  return true;
}

void PopupBlockerTabHelper::AddBlockedPopup(const BlockedWindowParams& params) {
  AddBlockedPopup(params.CreateNavigateParams(web_contents()),
                  params.features());
}

void PopupBlockerTabHelper::AddBlockedPopup(
    const chrome::NavigateParams& params,
    const WebWindowFeatures& window_features) {
  if (blocked_popups_.size() >= kMaximumNumberOfPopups)
    return;

  blocked_popups_.Add(
      base::MakeUnique<BlockedRequest>(params, window_features));
  TabSpecificContentSettings::FromWebContents(web_contents())->
      OnContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS);
}

void PopupBlockerTabHelper::ShowBlockedPopup(int32_t id) {
  BlockedRequest* popup = blocked_popups_.Lookup(id);
  if (!popup)
    return;
  // We set user_gesture to true here, so the new popup gets correctly focused.
  popup->params.user_gesture = true;
#if BUILDFLAG(ANDROID_JAVA_UI)
  TabModelList::HandlePopupNavigation(&popup->params);
#else
  chrome::Navigate(&popup->params);
#endif
  if (popup->params.target_contents) {
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
  for (IDMap<BlockedRequest, IDMapOwnPointer>::const_iterator iter(
           &blocked_popups_);
       !iter.IsAtEnd();
       iter.Advance()) {
    result[iter.GetCurrentKey()] = iter.GetCurrentValue()->params.url;
  }
  return result;
}
