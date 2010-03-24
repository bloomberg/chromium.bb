// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/store_timestamps_command.h"

#include "chrome/browser/sync/sessions/status_controller.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
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

  if (updates.has_new_timestamp()) {
    LOG(INFO) << "Get Updates got new timestamp: " << updates.new_timestamp();
    if (updates.new_timestamp() > dir->last_download_timestamp()) {
      dir->set_last_download_timestamp(updates.new_timestamp());
    }
  }

  // TODO(tim): Related to bug 30665, the Directory needs last sync timestamp
  // per data type.  Until then, use UNSPECIFIED.
  status->set_current_sync_timestamp(syncable::UNSPECIFIED,
                                     dir->last_download_timestamp());
}

}  // namespace browser_sync
