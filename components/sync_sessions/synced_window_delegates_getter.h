// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATES_GETTER_H_
#define COMPONENTS_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATES_GETTER_H_

#include <map>

#include "base/macros.h"
#include "components/sessions/core/session_id.h"

namespace sync_sessions {

class SyncedWindowDelegate;

// An interface for accessing SyncedWindowDelegates. Subclasses define
// how this is done on different platforms.
class SyncedWindowDelegatesGetter {
 public:
  using SyncedWindowDelegateMap =
      std::map<SessionID, const SyncedWindowDelegate*>;

  SyncedWindowDelegatesGetter();
  virtual ~SyncedWindowDelegatesGetter();

  // Returns all SyncedWindowDelegate instances.
  virtual SyncedWindowDelegateMap GetSyncedWindowDelegates() = 0;

  // Find a SyncedWindowDelegate given its window's id.
  virtual const SyncedWindowDelegate* FindById(SessionID id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedWindowDelegatesGetter);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATES_GETTER_H_
