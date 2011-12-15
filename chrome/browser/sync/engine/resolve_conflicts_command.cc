// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/resolve_conflicts_command.h"

#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"

namespace browser_sync {

ResolveConflictsCommand::ResolveConflictsCommand() {}
ResolveConflictsCommand::~ResolveConflictsCommand() {}

std::set<ModelSafeGroup> ResolveConflictsCommand::GetGroupsToChange(
    const sessions::SyncSession& session) const {
  return session.GetEnabledGroupsWithConflicts();
}

void ResolveConflictsCommand::ModelChangingExecuteImpl(
    sessions::SyncSession* session) {
  ConflictResolver* resolver = session->context()->resolver();
  DCHECK(resolver);
  if (!resolver)
    return;

  syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                session->context()->account_name());
  if (!dir.good())
    return;
  sessions::StatusController* status = session->mutable_status_controller();
  const sessions::ConflictProgress* progress = status->conflict_progress();
  if (!progress)
    return;  // Nothing to do.
  status->update_conflicts_resolved(
      resolver->ResolveConflicts(dir, *progress, status));
}

}  // namespace browser_sync
