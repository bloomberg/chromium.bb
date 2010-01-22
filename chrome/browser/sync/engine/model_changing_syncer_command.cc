// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/model_changing_syncer_command.h"

#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/util/closure.h"

namespace browser_sync {

void ModelChangingSyncerCommand::ExecuteImpl(sessions::SyncSession* session) {
  work_session_ = session;
  for (size_t i = 0; i < session->workers().size(); ++i) {
    ModelSafeWorker* worker = session->workers()[i];
    ModelSafeGroup group = worker->GetModelSafeGroup();

    sessions::ScopedModelSafeGroupRestriction r(work_session_, group);
    worker->DoWorkAndWaitUntilDone(NewCallback(this,
        &ModelChangingSyncerCommand::StartChangingModel));
  }
}

}  // namespace browser_sync
