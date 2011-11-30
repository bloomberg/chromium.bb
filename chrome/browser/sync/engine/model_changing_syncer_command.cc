// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/model_changing_syncer_command.h"

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "chrome/browser/sync/sessions/status_controller.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/util/unrecoverable_error_info.h"

namespace browser_sync {

void ModelChangingSyncerCommand::ExecuteImpl(sessions::SyncSession* session) {
  work_session_ = session;
  if (!ModelNeutralExecuteImpl(work_session_)) {
    return;
  }

  const std::set<ModelSafeGroup>& groups_to_change =
      GetGroupsToChange(*work_session_);
  for (size_t i = 0; i < session->workers().size(); ++i) {
    ModelSafeWorker* worker = work_session_->workers()[i];
    ModelSafeGroup group = worker->GetModelSafeGroup();
    // Skip workers whose group isn't active.
    if (groups_to_change.count(group) == 0u) {
      DVLOG(2) << "Skipping worker for group "
               << ModelSafeGroupToString(group);
      continue;
    }

    sessions::StatusController* status =
        work_session_->mutable_status_controller();
    sessions::ScopedModelSafeGroupRestriction r(status, group);
    WorkCallback c = base::Bind(
        &ModelChangingSyncerCommand::StartChangingModel,
        // We wait until the callback is executed. So it is safe to use
        // unretained.
        base::Unretained(this));

    // TODO(lipalani): Check the return value for an unrecoverable error.
    ignore_result(worker->DoWorkAndWaitUntilDone(c));

  }
}

bool ModelChangingSyncerCommand::ModelNeutralExecuteImpl(
    sessions::SyncSession* session) {
  return true;
}

}  // namespace browser_sync
