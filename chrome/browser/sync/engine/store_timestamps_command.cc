// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/store_timestamps_command.h"

#include "chrome/browser/sync/sessions/status_controller.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

StoreTimestampsCommand::StoreTimestampsCommand() {}
StoreTimestampsCommand::~StoreTimestampsCommand() {}

void StoreTimestampsCommand::ExecuteImpl(sessions::SyncSession* session) {
  syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                session->context()->account_name());

  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  const GetUpdatesResponse& updates =
      session->status_controller().updates_response().get_updates();

  sessions::StatusController* status = session->mutable_status_controller();

  // Update the progress marker tokens from the server result.  If a marker
  // was omitted for any one type, that indicates no change from the previous
  // state.
  syncable::ModelTypeBitSet forward_progress_types;
  for (int i = 0; i < updates.new_progress_marker_size(); ++i) {
    syncable::ModelType model =
        syncable::GetModelTypeFromExtensionFieldNumber(
            updates.new_progress_marker(i).data_type_id());
    if (model == syncable::UNSPECIFIED || model == syncable::TOP_LEVEL_FOLDER) {
      NOTREACHED() << "Unintelligible server response.";
      continue;
    }
    forward_progress_types[model] = true;
    dir->SetDownloadProgress(model, updates.new_progress_marker(i));
  }
  DCHECK(forward_progress_types.any() ||
         updates.changes_remaining() == 0);
  if (VLOG_IS_ON(1)) {
    DVLOG_IF(1, forward_progress_types.any())
        << "Get Updates got new progress marker for types: "
        << forward_progress_types.to_string() << " out of possible: "
        << status->updates_request_types().to_string();
  }
  if (updates.has_changes_remaining()) {
    int64 changes_left = updates.changes_remaining();
    DVLOG(1) << "Changes remaining: " << changes_left;
    status->set_num_server_changes_remaining(changes_left);
  }
}

}  // namespace browser_sync
