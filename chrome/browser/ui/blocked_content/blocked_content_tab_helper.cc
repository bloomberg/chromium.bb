// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"

#include "base/auto_reset.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_content_container.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"

BlockedContentTabHelper::BlockedContentTabHelper(
    TabContentsWrapper* tab_contents)
        : TabContentsObserver(tab_contents->tab_contents()),
          blocked_contents_(new BlockedContentContainer(tab_contents)),
          all_contents_blocked_(false),
          tab_contents_wrapper_(tab_contents),
          delegate_(NULL) {
}

BlockedContentTabHelper::~BlockedContentTabHelper() {
}

void BlockedContentTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Clear all page actions, blocked content notifications and browser actions
  // for this tab, unless this is an in-page navigation.
  if (!details.is_in_page) {
    // Close blocked popups.
    if (blocked_contents_->GetBlockedContentsCount()) {
      blocked_contents_->Clear();
      PopupNotificationVisibilityChanged(false);
    }
  }
}

void BlockedContentTabHelper::PopupNotificationVisibilityChanged(
    bool visible) {
  if (tab_contents()->is_being_destroyed())
    return;
  tab_contents_wrapper_->content_settings()->SetPopupsBlocked(visible);
}

void BlockedContentTabHelper::SetAllContentsBlocked(bool value) {
  if (all_contents_blocked_ == value)
    return;

  all_contents_blocked_ = value;
  if (!all_contents_blocked_ && blocked_contents_->GetBlockedContentsCount()) {
    std::vector<TabContentsWrapper*> blocked;
    blocked_contents_->GetBlockedContents(&blocked);
    for (size_t i = 0; i < blocked.size(); ++i)
      blocked_contents_->LaunchForContents(blocked[i]);
  }
}

void BlockedContentTabHelper::AddTabContents(TabContentsWrapper* new_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_pos,
                                             bool user_gesture) {
  if (!blocked_contents_->GetBlockedContentsCount())
    PopupNotificationVisibilityChanged(true);
  blocked_contents_->AddTabContents(
      new_contents, disposition, initial_pos, user_gesture);
}

void BlockedContentTabHelper::AddPopup(TabContentsWrapper* new_contents,
                                       const gfx::Rect& initial_pos,
                                       bool user_gesture) {
  // A page can't spawn popups (or do anything else, either) until its load
  // commits, so when we reach here, the popup was spawned by the
  // NavigationController's last committed entry, not the active entry.  For
  // example, if a page opens a popup in an onunload() handler, then the active
  // entry is the page to be loaded as we navigate away from the unloading
  // page.  For this reason, we can't use GetURL() to get the opener URL,
  // because it returns the active entry.
  NavigationEntry* entry = tab_contents()->controller().GetLastCommittedEntry();
  GURL creator = entry ? entry->virtual_url() : GURL::EmptyGURL();
  Profile* profile =
      Profile::FromBrowserContext(tab_contents()->browser_context());

  if (creator.is_valid() &&
      profile->GetHostContentSettingsMap()->GetContentSetting(
          creator,
          creator,
          CONTENT_SETTINGS_TYPE_POPUPS,
          "") == CONTENT_SETTING_ALLOW) {
    tab_contents()->AddNewContents(new_contents->tab_contents(),
                                   NEW_POPUP,
                                   initial_pos,
                                   true);  // user_gesture
  } else {
    // Call blocked_contents_->AddTabContents with user_gesture == true
    // so that the contents will not get blocked again.
    blocked_contents_->AddTabContents(new_contents,
                                      NEW_POPUP,
                                      initial_pos,
                                      true);  // user_gesture
    tab_contents_wrapper_->content_settings()->OnContentBlocked(
          CONTENT_SETTINGS_TYPE_POPUPS, std::string());
  }
}

void BlockedContentTabHelper::LaunchForContents(
    TabContentsWrapper* tab_contents) {
  blocked_contents_->LaunchForContents(tab_contents);
  if (!blocked_contents_->GetBlockedContentsCount())
    PopupNotificationVisibilityChanged(false);
}

size_t BlockedContentTabHelper::GetBlockedContentsCount() const {
  return blocked_contents_->GetBlockedContentsCount();
}

void BlockedContentTabHelper::GetBlockedContents(
    std::vector<TabContentsWrapper*>* blocked_contents) const {
  blocked_contents_->GetBlockedContents(blocked_contents);
}
