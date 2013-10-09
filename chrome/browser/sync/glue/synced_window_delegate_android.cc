// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_window_delegate_android.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"

namespace browser_sync {

// SyncedWindowDelegate implementations

const std::set<SyncedWindowDelegate*>
    SyncedWindowDelegate::GetSyncedWindowDelegates() {
  std::set<SyncedWindowDelegate*> synced_window_delegates;
  for (TabModelList::const_iterator i = TabModelList::begin();
      i != TabModelList::end(); ++i) {
    synced_window_delegates.insert((*i)->GetSyncedWindowDelegate());
  }
  return synced_window_delegates;
}

const SyncedWindowDelegate*
    SyncedWindowDelegate::FindSyncedWindowDelegateWithId(
        SessionID::id_type session_id) {
  TabModel* tab_model = TabModelList::FindTabModelWithId(
      session_id);

  // In case we don't find the browser (e.g. for Developer Tools).
  return tab_model ? tab_model->GetSyncedWindowDelegate() : NULL;
}

// SyncedWindowDelegateAndroid implementations

SyncedWindowDelegateAndroid::SyncedWindowDelegateAndroid(
    TabModel* tab_model)
    : tab_model_(tab_model) {}

SyncedWindowDelegateAndroid::~SyncedWindowDelegateAndroid() {}

bool SyncedWindowDelegateAndroid::HasWindow() const {
  return !tab_model_->IsOffTheRecord();
}

SessionID::id_type SyncedWindowDelegateAndroid::GetSessionId() const {
  return tab_model_->GetSessionId();
}

int SyncedWindowDelegateAndroid::GetTabCount() const {
  return tab_model_->GetTabCount();
}

int SyncedWindowDelegateAndroid::GetActiveIndex() const {
  return tab_model_->GetActiveIndex();
}

bool SyncedWindowDelegateAndroid::IsApp() const {
  return false;
}

bool SyncedWindowDelegateAndroid::IsTypeTabbed() const {
  return true;
}

bool SyncedWindowDelegateAndroid::IsTypePopup() const {
  return false;
}

bool SyncedWindowDelegateAndroid::IsTabPinned(
    const SyncedTabDelegate* tab) const {
  return false;
}

SyncedTabDelegate* SyncedWindowDelegateAndroid::GetTabAt(int index) const {
  // After a restart, it is possible for the Tab to be null during startup.
  TabAndroid* tab = tab_model_->GetTabAt(index);
  return tab ? tab->GetSyncedTabDelegate() : NULL;
}

SessionID::id_type SyncedWindowDelegateAndroid::GetTabIdAt(int index) const {
  SyncedTabDelegate* tab = GetTabAt(index);
  return tab ? tab->GetSessionId() : -1;
}

bool SyncedWindowDelegateAndroid::IsSessionRestoreInProgress() const {
  return tab_model_->IsSessionRestoreInProgress();
}

}  // namespace browser_sync
