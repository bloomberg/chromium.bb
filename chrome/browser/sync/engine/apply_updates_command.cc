// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/apply_updates_command.h"

#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/update_applicator.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/sync_types.h"

namespace browser_sync {

ApplyUpdatesCommand::ApplyUpdatesCommand() {}
ApplyUpdatesCommand::~ApplyUpdatesCommand() {}

void ApplyUpdatesCommand::ModelChangingExecuteImpl(SyncerSession *session) {
  syncable::ScopedDirLookup dir(session->dirman(), session->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }
  syncable::WriteTransaction trans(dir, syncable::SYNCER, __FILE__, __LINE__);
  syncable::Directory::UnappliedUpdateMetaHandles handles;
  dir->GetUnappliedUpdateMetaHandles(&trans, &handles);

  UpdateApplicator applicator(session->resolver(), handles.begin(),
                              handles.end());
  while (applicator.AttemptOneApplication(&trans)) {}
  applicator.SaveProgressIntoSessionState(session);
}

}  // namespace browser_sync
