// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncer_status.h"

namespace browser_sync {
SyncerStatus::SyncerStatus(SyncerSession* s) {
  sync_process_state_ = s->sync_process_state_;
  sync_cycle_state_ = s->sync_cycle_state_;
}
SyncerStatus::~SyncerStatus() {}

}  // namespace browser_sync
