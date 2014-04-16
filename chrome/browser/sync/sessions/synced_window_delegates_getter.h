// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATES_GETTER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATES_GETTER_H_

#include <set>
#include "base/macros.h"

namespace browser_sync {

class SyncedWindowDelegate;

class SyncedWindowDelegatesGetter {
 public:
  SyncedWindowDelegatesGetter();
  virtual ~SyncedWindowDelegatesGetter();
  virtual const std::set<SyncedWindowDelegate*> GetSyncedWindowDelegates();
 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedWindowDelegatesGetter);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATES_GETTER_H_
