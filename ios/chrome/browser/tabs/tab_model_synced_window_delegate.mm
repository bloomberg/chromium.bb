// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"

#include <set>

#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
#include "ios/chrome/browser/tabs/tab.h"
#include "ios/chrome/browser/tabs/tab_model.h"
#import "ios/web/public/web_state/web_state.h"

TabModelSyncedWindowDelegate::TabModelSyncedWindowDelegate(TabModel* tab_model)
    : tab_model_(tab_model) {}

TabModelSyncedWindowDelegate::~TabModelSyncedWindowDelegate() {}

bool TabModelSyncedWindowDelegate::IsTabPinned(
    const sync_sessions::SyncedTabDelegate* tab) const {
  return false;
}

sync_sessions::SyncedTabDelegate* TabModelSyncedWindowDelegate::GetTabAt(
    int index) const {
  sync_sessions::SyncedTabDelegate* delegate =
      IOSChromeSyncedTabDelegate::FromWebState(
          [tab_model_ tabAtIndex:index].webState);
  if (!delegate) {
    return nullptr;
  }
  return delegate;
}

SessionID::id_type TabModelSyncedWindowDelegate::GetTabIdAt(int index) const {
  return GetTabAt(index)->GetSessionId();
}

bool TabModelSyncedWindowDelegate::IsSessionRestoreInProgress() const {
  return false;
}

bool TabModelSyncedWindowDelegate::HasWindow() const {
  return true;
}

SessionID::id_type TabModelSyncedWindowDelegate::GetSessionId() const {
  return tab_model_.sessionID.id();
}

int TabModelSyncedWindowDelegate::GetTabCount() const {
  return [tab_model_ count];
}

int TabModelSyncedWindowDelegate::GetActiveIndex() const {
  Tab* current_tab = [tab_model_ currentTab];
  DCHECK(current_tab);
  return [tab_model_ indexOfTab:current_tab];
}

bool TabModelSyncedWindowDelegate::IsApp() const {
  return false;
}

bool TabModelSyncedWindowDelegate::IsTypeTabbed() const {
  return true;
}

bool TabModelSyncedWindowDelegate::IsTypePopup() const {
  return false;
}

bool TabModelSyncedWindowDelegate::ShouldSync() const {
  return true;
}
