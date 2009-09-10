// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/resolve_conflicts_command.h"

#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"

namespace browser_sync {

ResolveConflictsCommand::ResolveConflictsCommand() {}
ResolveConflictsCommand::~ResolveConflictsCommand() {}

void ResolveConflictsCommand::ModelChangingExecuteImpl(
    SyncerSession* session) {
  if (!session->resolver())
    return;
  syncable::ScopedDirLookup dir(session->dirman(), session->account_name());
  if (!dir.good())
    return;
  ConflictResolutionView conflict_view(session);
  session->set_conflicts_resolved(
      session->resolver()->ResolveConflicts(dir, &conflict_view, session));
}

}  // namespace browser_sync
