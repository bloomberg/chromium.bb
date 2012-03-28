// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_window_delegate.h"

#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sync/glue/synced_window_delegate_registry.h"

namespace browser_sync {

const std::set<SyncedWindowDelegate*>
    SyncedWindowDelegate::GetSyncedWindowDelegates() {
  return SyncedWindowDelegateRegistry::GetSyncedWindowDelegates();
}

const SyncedWindowDelegate*
    SyncedWindowDelegate::FindSyncedWindowDelegateWithId(
        SessionID::id_type session_id) {
  std::set<SyncedWindowDelegate*> window =
      SyncedWindowDelegateRegistry::GetSyncedWindowDelegates();
  for (std::set<SyncedWindowDelegate*>::const_iterator i =
           window.begin(); i != window.end(); ++i) {
    if ((*i)->GetSessionId() == session_id)
      return *i;
  }
  return NULL;
}

}  // namespace browser_sync
