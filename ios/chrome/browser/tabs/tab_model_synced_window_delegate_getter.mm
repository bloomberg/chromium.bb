// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/tabs/tab_model_synced_window_delegate_getter.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"
#import "ios/chrome/browser/ui/browser_list_ios.h"

TabModelSyncedWindowDelegatesGetter::TabModelSyncedWindowDelegatesGetter(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}

TabModelSyncedWindowDelegatesGetter::~TabModelSyncedWindowDelegatesGetter() {}

std::set<const sync_sessions::SyncedWindowDelegate*>
TabModelSyncedWindowDelegatesGetter::GetSyncedWindowDelegates() {
  std::set<const sync_sessions::SyncedWindowDelegate*> synced_window_delegates;

  for (BrowserListIOS::const_iterator iter = BrowserListIOS::begin();
       iter != BrowserListIOS::end(); ++iter) {
    id<BrowserIOS> browser = *iter;
    TabModel* tabModel = [browser tabModel];
    // TODO(crbug.com/548612): BrowserState may be unnecessary as iOS does not
    // support multiple profiles starting with M47. There should still be a way
    // to filter out Incognito delegates, though.
    if (tabModel.browserState != browser_state_)
      continue;
    // Do not return windows without any tabs, to match desktop.
    if ([tabModel currentTab])
      synced_window_delegates.insert([tabModel syncedWindowDelegate]);
  }

  return synced_window_delegates;
}

const sync_sessions::SyncedWindowDelegate*
TabModelSyncedWindowDelegatesGetter::FindById(SessionID::id_type session_id) {
  for (const sync_sessions::SyncedWindowDelegate* delegate :
       GetSyncedWindowDelegates()) {
    if (session_id == delegate->GetSessionId())
      return delegate;
  }
  return nullptr;
}
