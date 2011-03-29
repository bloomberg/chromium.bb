// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/apply_updates_command.h"

#include "chrome/browser/sync/engine/update_applicator.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

using sessions::SyncSession;

ApplyUpdatesCommand::ApplyUpdatesCommand() {}
ApplyUpdatesCommand::~ApplyUpdatesCommand() {}

void ApplyUpdatesCommand::ModelChangingExecuteImpl(SyncSession* session) {
  syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }
  syncable::WriteTransaction trans(dir, syncable::SYNCER, __FILE__, __LINE__);
  syncable::Directory::UnappliedUpdateMetaHandles handles;
  dir->GetUnappliedUpdateMetaHandles(&trans, &handles);

  UpdateApplicator applicator(
      session->context()->resolver(),
      session->context()->directory_manager()->GetCryptographer(&trans),
      handles.begin(), handles.end(), session->routing_info(),
      session->status_controller()->group_restriction());
  while (applicator.AttemptOneApplication(&trans)) {}
  applicator.SaveProgressIntoSessionState(
      session->status_controller()->mutable_conflict_progress(),
      session->status_controller()->mutable_update_progress());

  // This might be the first time we've fully completed a sync cycle, for
  // some subset of the currently synced datatypes.
  sessions::StatusController* status(session->status_controller());
  if (status->ServerSaysNothingMoreToDownload()) {
    syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                  session->context()->account_name());
    if (!dir.good()) {
      LOG(ERROR) << "Scoped dir lookup failed!";
      return;
    }

    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
         i < syncable::MODEL_TYPE_COUNT; ++i) {
      syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
      if (status->updates_request_types()[i]) {
        // This gets persisted to the directory's backing store.
        dir->set_initial_sync_ended_for_type(model_type, true);
      }
    }
  }
}

}  // namespace browser_sync
