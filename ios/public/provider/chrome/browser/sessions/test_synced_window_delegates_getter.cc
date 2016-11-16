// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/sessions/test_synced_window_delegates_getter.h"

std::set<const sync_sessions::SyncedWindowDelegate*>
TestSyncedWindowDelegatesGetter::GetSyncedWindowDelegates() {
  return std::set<const sync_sessions::SyncedWindowDelegate*>();
}

const sync_sessions::SyncedWindowDelegate*
TestSyncedWindowDelegatesGetter::FindById(SessionID::id_type id) {
  return nullptr;
}
