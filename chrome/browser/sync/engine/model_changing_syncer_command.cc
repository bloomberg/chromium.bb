// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/model_changing_syncer_command.h"

#include "base/callback.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/sessions/status_controller.h"
#include "chrome/browser/sync/sessions/sync_session.h"

namespace browser_sync {

void ModelChangingSyncerCommand::ExecuteImpl(sessions::SyncSession* session) {
  work_session_ = session;
  if (!ModelNeutralExecuteImpl(work_session_)) {
    return;
  }

  for (size_t i = 0; i < session->workers().size(); ++i) {
    ModelSafeWorker* worker = session->workers()[i];
    ModelSafeGroup group = worker->GetModelSafeGroup();

    sessions::StatusController* status = work_session_->status_controller();
    sessions::ScopedModelSafeGroupRestriction r(status, group);
    scoped_ptr<Callback0::Type> c(NewCallback(this,
        &ModelChangingSyncerCommand::StartChangingModel));
    worker->DoWorkAndWaitUntilDone(c.get());
  }
}

bool ModelChangingSyncerCommand::ModelNeutralExecuteImpl(
    sessions::SyncSession* session) {
  return true;
}

}  // namespace browser_sync
