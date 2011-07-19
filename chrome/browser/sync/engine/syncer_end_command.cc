// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  // Always send out a cycle ended notification, regardless of end-state.
  session->status_controller()->set_syncing(false);
  SyncEngineEvent event(SyncEngineEvent::SYNC_CYCLE_ENDED);
  sessions::SyncSessionSnapshot snapshot(session->TakeSnapshot());
  event.snapshot = &snapshot;
  session->context()->NotifyListeners(event);
  VLOG(1) << this << " sent sync end snapshot";
}

}  // namespace browser_sync
