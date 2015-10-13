// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_window_delegates_getter_android.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "components/sync_driver/glue/synced_window_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::NavigationEntry;

namespace browser_sync {
SyncedTabDelegateAndroid::SyncedTabDelegateAndroid(TabAndroid* tab_android)
    : web_contents_(nullptr),
      tab_android_(tab_android),
      tab_contents_delegate_(nullptr) {
  SetSyncedWindowGetter(
      make_scoped_ptr(new SyncedWindowDelegatesGetterAndroid()));
}

SyncedTabDelegateAndroid::~SyncedTabDelegateAndroid() {}

SessionID::id_type SyncedTabDelegateAndroid::GetWindowId() const {
  return tab_contents_delegate_->GetWindowId();
}

SessionID::id_type SyncedTabDelegateAndroid::GetSessionId() const {
  return tab_android_->session_id().id();
}

bool SyncedTabDelegateAndroid::IsBeingDestroyed() const {
  return tab_contents_delegate_->IsBeingDestroyed();
}

Profile* SyncedTabDelegateAndroid::profile() const {
  return tab_contents_delegate_->profile();
}

std::string SyncedTabDelegateAndroid::GetExtensionAppId() const {
  return tab_contents_delegate_->GetExtensionAppId();
}

bool SyncedTabDelegateAndroid::IsInitialBlankNavigation() const {
  return tab_contents_delegate_->IsInitialBlankNavigation();
}

int SyncedTabDelegateAndroid::GetCurrentEntryIndex() const {
  return tab_contents_delegate_->GetCurrentEntryIndex();
}

int SyncedTabDelegateAndroid::GetEntryCount() const {
  return tab_contents_delegate_->GetEntryCount();
}

int SyncedTabDelegateAndroid::GetPendingEntryIndex() const {
  return tab_contents_delegate_->GetPendingEntryIndex();
}

NavigationEntry* SyncedTabDelegateAndroid::GetPendingEntry() const {
  return tab_contents_delegate_->GetPendingEntry();
}

NavigationEntry* SyncedTabDelegateAndroid::GetEntryAtIndex(int i) const {
  return tab_contents_delegate_->GetEntryAtIndex(i);
}

NavigationEntry* SyncedTabDelegateAndroid::GetActiveEntry() const {
  return tab_contents_delegate_->GetActiveEntry();
}

bool SyncedTabDelegateAndroid::IsPinned() const {
  return tab_contents_delegate_->IsPinned();
}

bool SyncedTabDelegateAndroid::HasWebContents() const {
  return web_contents_ != NULL;
}

content::WebContents* SyncedTabDelegateAndroid::GetWebContents() const {
  return web_contents_;
}

void SyncedTabDelegateAndroid::SetWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents;
  TabContentsSyncedTabDelegate::CreateForWebContents(web_contents_);
  // Store the TabContentsSyncedTabDelegate object that was created.
  tab_contents_delegate_ =
      TabContentsSyncedTabDelegate::FromWebContents(web_contents_);
  // Tell it how to get SyncedWindowDelegates or some calls will fail.
  tab_contents_delegate_->SetSyncedWindowGetter(
      make_scoped_ptr(new SyncedWindowDelegatesGetterAndroid()));
}

void SyncedTabDelegateAndroid::ResetWebContents() { web_contents_ = NULL; }

bool SyncedTabDelegateAndroid::ProfileIsSupervised() const {
  return tab_contents_delegate_->ProfileIsSupervised();
}

const std::vector<const content::NavigationEntry*>*
SyncedTabDelegateAndroid::GetBlockedNavigations() const {
  return tab_contents_delegate_->GetBlockedNavigations();
}

int SyncedTabDelegateAndroid::GetSyncId() const {
  return tab_android_->GetSyncId();
}

void SyncedTabDelegateAndroid::SetSyncId(int sync_id) {
  tab_android_->SetSyncId(sync_id);
}

// static
SyncedTabDelegate* SyncedTabDelegate::ImplFromWebContents(
    content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab ? tab->GetSyncedTabDelegate() : nullptr;
}

}  // namespace browser_sync
