// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PopupBlockerTabHelper);

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
    const chrome::NavigateParams& params) {
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
      profile->GetHostContentSettingsMap()->GetContentSetting(
          creator, creator, CONTENT_SETTINGS_TYPE_POPUPS, std::string()) ==
          CONTENT_SETTING_ALLOW) {
    return false;
  } else {
    blocked_popups_.Add(new chrome::NavigateParams(params));
    TabSpecificContentSettings::FromWebContents(web_contents())->
        OnContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS, std::string());
    return true;
  }
}

void PopupBlockerTabHelper::ShowBlockedPopup(int32 id) {
  chrome::NavigateParams* params = blocked_popups_.Lookup(id);
  if (!params)
    return;
  chrome::Navigate(params);
  blocked_popups_.Remove(id);
  if (blocked_popups_.IsEmpty())
    PopupNotificationVisibilityChanged(false);
}

size_t PopupBlockerTabHelper::GetBlockedPopupsCount() const {
  return blocked_popups_.size();
}

IDMap<chrome::NavigateParams, IDMapOwnPointer>&
PopupBlockerTabHelper::GetBlockedPopupRequests() {
  return blocked_popups_;
}
