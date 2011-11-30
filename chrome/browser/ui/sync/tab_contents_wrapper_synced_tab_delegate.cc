// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_wrapper_synced_tab_delegate.h"

#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"


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
  return tab_contents_wrapper_->tab_contents()->is_being_destroyed();
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
  return tab_contents_wrapper_->controller().GetCurrentEntryIndex();
}

int TabContentsWrapperSyncedTabDelegate::GetEntryCount() const {
  return tab_contents_wrapper_->controller().entry_count();
}

int TabContentsWrapperSyncedTabDelegate::GetPendingEntryIndex() const {
  return tab_contents_wrapper_->controller().pending_entry_index();
}

NavigationEntry* TabContentsWrapperSyncedTabDelegate::GetPendingEntry() const {
  return tab_contents_wrapper_->controller().pending_entry();
}

NavigationEntry* TabContentsWrapperSyncedTabDelegate::GetEntryAtIndex(int i)
    const {
  return tab_contents_wrapper_->controller().GetEntryAtIndex(i);
}

NavigationEntry* TabContentsWrapperSyncedTabDelegate::GetActiveEntry() const {
  return tab_contents_wrapper_->controller().GetActiveEntry();
}
