// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/apply_updates_command.h"

#include "base/location.h"
#include "chrome/browser/sync/engine/update_applicator.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

using sessions::SyncSession;

ApplyUpdatesCommand::ApplyUpdatesCommand() {}
ApplyUpdatesCommand::~ApplyUpdatesCommand() {}

bool ApplyUpdatesCommand::HasCustomGroupsToChange() const {
  // TODO(akalin): Set to true.
  return false;
}

std::set<ModelSafeGroup> ApplyUpdatesCommand::GetGroupsToChange(
    const sessions::SyncSession& session) const {
  std::set<ModelSafeGroup> groups_with_unapplied_updates;

  syncable::ModelTypeBitSet server_types_with_unapplied_updates;
  {
    syncable::ScopedDirLookup dir(session.context()->directory_manager(),
                                  session.context()->account_name());
    if (!dir.good()) {
      LOG(ERROR) << "Scoped dir lookup failed!";
      return groups_with_unapplied_updates;
    }

    syncable::ReadTransaction trans(FROM_HERE, dir);
    server_types_with_unapplied_updates =
        dir->GetServerTypesWithUnappliedUpdates(&trans);
  }

  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    const syncable::ModelType type = syncable::ModelTypeFromInt(i);
    if (server_types_with_unapplied_updates.test(type)) {
      groups_with_unapplied_updates.insert(
          GetGroupForModelType(type, session.routing_info()));
    }
  }

  return groups_with_unapplied_updates;
}

void ApplyUpdatesCommand::ModelChangingExecuteImpl(SyncSession* session) {
  syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER, dir);

  // Compute server types with unapplied updates that fall under our
  // group restriction.
  const syncable::ModelTypeBitSet server_types_with_unapplied_updates =
      dir->GetServerTypesWithUnappliedUpdates(&trans);
  syncable::ModelTypeBitSet server_type_restriction;
  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    const syncable::ModelType server_type = syncable::ModelTypeFromInt(i);
    if (server_types_with_unapplied_updates.test(server_type)) {
      if (GetGroupForModelType(server_type, session->routing_info()) ==
          session->status_controller().group_restriction()) {
        server_type_restriction.set(server_type);
      }
    }
  }

  syncable::Directory::UnappliedUpdateMetaHandles handles;
  dir->GetUnappliedUpdateMetaHandles(
      &trans, server_type_restriction, &handles);

  UpdateApplicator applicator(
      session->context()->resolver(),
      session->context()->directory_manager()->GetCryptographer(&trans),
      handles.begin(), handles.end(), session->routing_info(),
      session->status_controller().group_restriction());
  while (applicator.AttemptOneApplication(&trans)) {}
  applicator.SaveProgressIntoSessionState(
      session->mutable_status_controller()->mutable_conflict_progress(),
      session->mutable_status_controller()->mutable_update_progress());

  // This might be the first time we've fully completed a sync cycle, for
  // some subset of the currently synced datatypes.
  const sessions::StatusController& status(session->status_controller());
  if (status.ServerSaysNothingMoreToDownload()) {
    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
         i < syncable::MODEL_TYPE_COUNT; ++i) {
      syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
      if (status.updates_request_types()[i]) {
        // This gets persisted to the directory's backing store.
        dir->set_initial_sync_ended_for_type(model_type, true);
      }
    }
  }
}

}  // namespace browser_sync
