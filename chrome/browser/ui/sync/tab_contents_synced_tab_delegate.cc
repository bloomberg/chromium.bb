// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::NavigationEntry;

TabContentsSyncedTabDelegate::TabContentsSyncedTabDelegate(
    TabContents* tab_contents)
        : tab_contents_(tab_contents) {}

TabContentsSyncedTabDelegate::~TabContentsSyncedTabDelegate() {}

SessionID::id_type TabContentsSyncedTabDelegate::GetWindowId() const {
  return SessionTabHelper::FromWebContents(tab_contents_->web_contents())->
      window_id().id();
}

SessionID::id_type TabContentsSyncedTabDelegate::GetSessionId() const {
  return SessionTabHelper::FromWebContents(tab_contents_->web_contents())->
      session_id().id();
}

bool TabContentsSyncedTabDelegate::IsBeingDestroyed() const {
  return tab_contents_->in_destructor();
}

Profile* TabContentsSyncedTabDelegate::profile() const {
  return tab_contents_->profile();
}

bool TabContentsSyncedTabDelegate::HasExtensionAppId() const {
  return !!(extensions::TabHelper::FromWebContents(
      tab_contents_->web_contents())->extension_app());
}

const std::string& TabContentsSyncedTabDelegate::GetExtensionAppId()
    const {
  DCHECK(HasExtensionAppId());
  return extensions::TabHelper::FromWebContents(tab_contents_->web_contents())->
      extension_app()->id();
}

int TabContentsSyncedTabDelegate::GetCurrentEntryIndex() const {
  return tab_contents_->web_contents()->GetController().GetCurrentEntryIndex();
}

int TabContentsSyncedTabDelegate::GetEntryCount() const {
  return tab_contents_->web_contents()->GetController().GetEntryCount();
}

int TabContentsSyncedTabDelegate::GetPendingEntryIndex() const {
  return tab_contents_->web_contents()->GetController().GetPendingEntryIndex();
}

NavigationEntry* TabContentsSyncedTabDelegate::GetPendingEntry() const {
  return
      tab_contents_->web_contents()->GetController().GetPendingEntry();
}

NavigationEntry* TabContentsSyncedTabDelegate::GetEntryAtIndex(int i)
    const {
  return tab_contents_->web_contents()->GetController().GetEntryAtIndex(i);
}

NavigationEntry* TabContentsSyncedTabDelegate::GetActiveEntry() const {
  return tab_contents_->web_contents()->GetController().GetActiveEntry();
}
