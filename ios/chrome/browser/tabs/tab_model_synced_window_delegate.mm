// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"

#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabModelSyncedWindowDelegate::TabModelSyncedWindowDelegate(
    WebStateList* web_state_list,
    SessionID session_id)
    : web_state_list_(web_state_list), session_id_(session_id) {}

TabModelSyncedWindowDelegate::~TabModelSyncedWindowDelegate() {}

bool TabModelSyncedWindowDelegate::IsTabPinned(
    const sync_sessions::SyncedTabDelegate* tab) const {
  return false;
}

sync_sessions::SyncedTabDelegate* TabModelSyncedWindowDelegate::GetTabAt(
    int index) const {
  return IOSChromeSyncedTabDelegate::FromWebState(
      web_state_list_->GetWebStateAt(index));
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
  return session_id_.id();
}

int TabModelSyncedWindowDelegate::GetTabCount() const {
  return web_state_list_->count();
}

int TabModelSyncedWindowDelegate::GetActiveIndex() const {
  DCHECK_NE(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  return web_state_list_->active_index();
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
