// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_end_command.h"

#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/common/deprecated/event_sys-inl.h"

namespace browser_sync {

SyncerEndCommand::SyncerEndCommand() {}
SyncerEndCommand::~SyncerEndCommand() {}

void SyncerEndCommand::ExecuteImpl(sessions::SyncSession* session) {
  sessions::StatusController* status(session->status_controller());
  status->set_syncing(false);
  session->context()->set_previous_session_routing_info(
      session->routing_info());
  session->context()->set_last_snapshot(session->TakeSnapshot());
}

}  // namespace browser_sync
