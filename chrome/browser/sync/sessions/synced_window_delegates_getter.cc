// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/synced_window_delegates_getter.h"

#include "chrome/browser/sync/glue/synced_window_delegate.h"

namespace browser_sync {

SyncedWindowDelegatesGetter::SyncedWindowDelegatesGetter() {}

SyncedWindowDelegatesGetter::~SyncedWindowDelegatesGetter() {}

const std::set<SyncedWindowDelegate*>
SyncedWindowDelegatesGetter::GetSyncedWindowDelegates() {
  return SyncedWindowDelegate::GetSyncedWindowDelegates();
}

}  // namespace browser_sync
