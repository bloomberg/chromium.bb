// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_window_delegate_registry.h"

#include "base/lazy_instance.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"

namespace browser_sync {

namespace {
base::LazyInstance<std::set<SyncedWindowDelegate*> >::Leaky g_delegates =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

/* static */
void SyncedWindowDelegateRegistry::Register(SyncedWindowDelegate* delegate) {
  g_delegates.Pointer()->insert(delegate);
}

/* static */
void SyncedWindowDelegateRegistry::Unregister(SyncedWindowDelegate* delegate) {
  g_delegates.Pointer()->erase(delegate);
}

/* static */
const std::set<SyncedWindowDelegate*>&
SyncedWindowDelegateRegistry::GetSyncedWindowDelegates() {
  return g_delegates.Get();
}

}  // namespace browser_sync
