// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
      session->status_controller()->updates_response().get_updates();

  sessions::StatusController* status = session->status_controller();
  if (updates.has_changes_remaining()) {
    int64 changes_left = updates.changes_remaining();
    LOG(INFO) << "Changes remaining:" << changes_left;
    status->set_num_server_changes_remaining(changes_left);
  }

  LOG_IF(INFO, updates.has_new_timestamp()) << "Get Updates got new timestamp: "
      << updates.new_timestamp() << " for type mask: "
      << status->updates_request_parameters().data_types.to_string();

  // Update the saved download timestamp for any items we fetched.
  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model = syncable::ModelTypeFromInt(i);
    if (status->updates_request_parameters().data_types[i] &&
        updates.has_new_timestamp() &&
        (updates.new_timestamp() > dir->last_download_timestamp(model))) {
      dir->set_last_download_timestamp(model, updates.new_timestamp());
    }
    status->set_current_download_timestamp(model,
        dir->last_download_timestamp(model));
  }
}

}  // namespace browser_sync
