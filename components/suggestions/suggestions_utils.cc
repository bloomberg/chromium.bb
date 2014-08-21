// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_utils.h"

namespace suggestions {

SyncState GetSyncState(bool sync_enabled,
                       bool sync_initialized,
                       bool history_sync_enabled) {
  if (!sync_enabled)
    return SYNC_OR_HISTORY_SYNC_DISABLED;

  if (!sync_initialized)
    return NOT_INITIALIZED_ENABLED;

  return history_sync_enabled ?
      INITIALIZED_ENABLED_HISTORY : SYNC_OR_HISTORY_SYNC_DISABLED;
}

}  // namespace suggestions
