// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_REGISTRY_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_REGISTRY_H_
#pragma once

#include <set>

namespace browser_sync {

class SyncedWindowDelegate;

// A location to register SyncedWindowDelegates such that Android can provide
// pseudo-browsers for the purposes of Sync without having a dependency from
// sync onto Android's model of a browser.
class SyncedWindowDelegateRegistry {
 public:
  static const std::set<SyncedWindowDelegate*>& GetSyncedWindowDelegates();
  static void Register(SyncedWindowDelegate* delegate);
  static void Unregister(SyncedWindowDelegate* delegate);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_REGISTRY_H_
