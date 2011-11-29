// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/model_changing_syncer_command.h"

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/sessions/status_controller.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/util/unrecoverable_error_info.h"

namespace browser_sync {

void ModelChangingSyncerCommand::ExecuteImpl(sessions::SyncSession* session) {
  work_session_ = session;
  if (!ModelNeutralExecuteImpl(work_session_)) {
    return;
  }

  // Project the list of active types (i.e., types in the routing
  // info) to a list of groups.
  //
  // TODO(akalin): Make this overrideable by subclasses (who might be
  // working on a subset of |active_groups|).  (See
  // http://crbug.com/97832.)
  std::set<ModelSafeGroup> active_groups;
  const ModelSafeRoutingInfo& routing_info = session->routing_info();
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    active_groups.insert(it->second);
  }
  // Always work on GROUP_PASSIVE, since that's the group that
  // top-level folders map to.
  active_groups.insert(GROUP_PASSIVE);

  for (size_t i = 0; i < session->workers().size(); ++i) {
    ModelSafeWorker* worker = session->workers()[i];
    ModelSafeGroup group = worker->GetModelSafeGroup();
    // Skip workers whose group isn't active.
    if (active_groups.find(group) == active_groups.end()) {
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
