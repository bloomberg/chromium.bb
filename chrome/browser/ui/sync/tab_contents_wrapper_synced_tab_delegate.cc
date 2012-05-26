// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_wrapper_synced_tab_delegate.h"

#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::NavigationEntry;

TabContentsWrapperSyncedTabDelegate::TabContentsWrapperSyncedTabDelegate(
    TabContentsWrapper* tab_contents_wrapper)
        : tab_contents_wrapper_(tab_contents_wrapper) {}

TabContentsWrapperSyncedTabDelegate::~TabContentsWrapperSyncedTabDelegate() {}

SessionID::id_type TabContentsWrapperSyncedTabDelegate::GetWindowId() const {
  return tab_contents_wrapper_->restore_tab_helper()->window_id().id();
}

SessionID::id_type TabContentsWrapperSyncedTabDelegate::GetSessionId() const {
  return tab_contents_wrapper_->restore_tab_helper()->session_id().id();
}

bool TabContentsWrapperSyncedTabDelegate::IsBeingDestroyed() const {
  return tab_contents_wrapper_->in_destructor();
}

Profile* TabContentsWrapperSyncedTabDelegate::profile() const {
  return tab_contents_wrapper_->profile();
}

bool TabContentsWrapperSyncedTabDelegate::HasExtensionAppId() const {
  return (tab_contents_wrapper_->extension_tab_helper() &&
      tab_contents_wrapper_->extension_tab_helper()->extension_app());
}

const std::string& TabContentsWrapperSyncedTabDelegate::GetExtensionAppId()
    const {
  DCHECK(HasExtensionAppId());
  return  tab_contents_wrapper_->extension_tab_helper()->extension_app()->id();
}

int TabContentsWrapperSyncedTabDelegate::GetCurrentEntryIndex() const {
  return tab_contents_wrapper_->web_contents()->GetController().
      GetCurrentEntryIndex();
}

int TabContentsWrapperSyncedTabDelegate::GetEntryCount() const {
  return tab_contents_wrapper_->web_contents()->GetController().GetEntryCount();
}

int TabContentsWrapperSyncedTabDelegate::GetPendingEntryIndex() const {
  return tab_contents_wrapper_->web_contents()->GetController().
      GetPendingEntryIndex();
}

NavigationEntry* TabContentsWrapperSyncedTabDelegate::GetPendingEntry() const {
  return
      tab_contents_wrapper_->web_contents()->GetController().GetPendingEntry();
}

NavigationEntry* TabContentsWrapperSyncedTabDelegate::GetEntryAtIndex(int i)
    const {
  return
      tab_contents_wrapper_->web_contents()->GetController().GetEntryAtIndex(i);
}

NavigationEntry* TabContentsWrapperSyncedTabDelegate::GetActiveEntry() const {
  return
      tab_contents_wrapper_->web_contents()->GetController().GetActiveEntry();
}
