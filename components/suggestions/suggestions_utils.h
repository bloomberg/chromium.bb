// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_SUGGESTIONS_UTILS_H_
#define COMPONENTS_SUGGESTIONS_SUGGESTIONS_UTILS_H_

namespace suggestions {

// Establishes the different sync states that users of SuggestionsService can
// specify. There are three different concepts in the sync service: initialized,
// sync enabled and history sync enabled.
enum SyncState {
  // State: Sync service is not initialized, yet not disabled. History sync
  //     state is unknown (since not initialized).
  // Behavior: Does not issue a server request, but serves from cache if
  //     available.
  NOT_INITIALIZED_ENABLED,

  // State: Sync service is initialized, sync is enabled and history sync is
  //     enabled.
  // Behavior: Update suggestions from the server. Serve from cache on timeout.
  INITIALIZED_ENABLED_HISTORY,

  // State: Sync service is disabled or history sync is disabled.
  // Behavior: Do not issue a server request. Clear the cache. Serve empty
  //     suggestions.
  SYNC_OR_HISTORY_SYNC_DISABLED,
};

// Users of SuggestionsService should always use this function to get SyncState.
SyncState GetSyncState(bool sync_enabled,
                       bool sync_initialized,
                       bool history_sync_enabled);

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_SUGGESTIONS_UTILS_H_
