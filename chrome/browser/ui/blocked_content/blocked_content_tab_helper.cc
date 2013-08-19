// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"

#include "base/auto_reset.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_content_container.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

using content::NavigationEntry;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(BlockedContentTabHelper);

BlockedContentTabHelper::BlockedContentTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      blocked_contents_(new BlockedContentContainer(web_contents)),
      all_contents_blocked_(false),
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
    if (blocked_contents_->GetBlockedContentsCount())
      blocked_contents_->Clear();
  }
}

void BlockedContentTabHelper::SendNotification(content::WebContents* contents,
                                               bool blocked_state) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CONTENT_BLOCKED_STATE_CHANGED,
      content::Source<content::WebContents>(contents),
      content::Details<const bool>(&blocked_state));
}

void BlockedContentTabHelper::SetAllContentsBlocked(bool value) {
  if (all_contents_blocked_ == value)
    return;

  all_contents_blocked_ = value;
  if (!all_contents_blocked_ && blocked_contents_->GetBlockedContentsCount()) {
    std::vector<content::WebContents*> blocked;
    blocked_contents_->GetBlockedContents(&blocked);
    for (size_t i = 0; i < blocked.size(); ++i) {
      SendNotification(blocked[i], false);
      blocked_contents_->LaunchForContents(blocked[i]);
    }
  }
}

void BlockedContentTabHelper::AddWebContents(content::WebContents* new_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_pos,
                                             bool user_gesture) {
  SendNotification(new_contents, true);
  blocked_contents_->AddWebContents(
      new_contents, disposition, initial_pos, user_gesture);
}
