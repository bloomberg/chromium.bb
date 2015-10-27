// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "components/sync_driver/glue/synced_window_delegate.h"

namespace browser_sync {

BrowserSyncedWindowDelegatesGetter::BrowserSyncedWindowDelegatesGetter(
    Profile* profile)
    : profile_(profile) {}
BrowserSyncedWindowDelegatesGetter::~BrowserSyncedWindowDelegatesGetter() {}

std::set<const SyncedWindowDelegate*>
BrowserSyncedWindowDelegatesGetter::GetSyncedWindowDelegates() {
  std::set<const SyncedWindowDelegate*> synced_window_delegates;
  // Add all the browser windows.
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (it->profile() != profile_)
      continue;
    synced_window_delegates.insert(it->synced_window_delegate());
  }
  return synced_window_delegates;
}

const SyncedWindowDelegate* BrowserSyncedWindowDelegatesGetter::FindById(
    SessionID::id_type id) {
  Browser* browser = chrome::FindBrowserWithID(id);
  return (browser != nullptr) ? browser->synced_window_delegate() : nullptr;
}

}  // namespace browser_sync
