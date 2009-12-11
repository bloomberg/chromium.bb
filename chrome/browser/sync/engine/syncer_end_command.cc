// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_end_command.h"

#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/event_sys-inl.h"

namespace browser_sync {

SyncerEndCommand::SyncerEndCommand() {}
SyncerEndCommand::~SyncerEndCommand() {}

void SyncerEndCommand::ExecuteImpl(sessions::SyncSession* session) {
  sessions::StatusController* status(session->status_controller());
  status->set_syncing(false);

  if (!session->HasMoreToSync()) {
    // This might be the first time we've fully completed a sync cycle.
    DCHECK(status->got_zero_updates());

    syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                  session->context()->account_name());
    if (!dir.good()) {
      LOG(ERROR) << "Scoped dir lookup failed!";
      return;
    }

    // This gets persisted to the directory's backing store.
    dir->set_initial_sync_ended(true);
  }

  SyncerEvent event(SyncerEvent::SYNC_CYCLE_ENDED);
  sessions::SyncSessionSnapshot snapshot(session->TakeSnapshot());
  event.snapshot = &snapshot;
  session->context()->syncer_event_channel()->NotifyListeners(event);
}

}  // namespace browser_sync
