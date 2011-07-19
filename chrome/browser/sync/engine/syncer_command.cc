// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_command.h"

#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/common/deprecated/event_sys-inl.h"

namespace browser_sync {
using sessions::SyncSession;

SyncerCommand::SyncerCommand() {}
SyncerCommand::~SyncerCommand() {}

void SyncerCommand::Execute(SyncSession* session) {
  ExecuteImpl(session);
  SendNotifications(session);
}

void SyncerCommand::SendNotifications(SyncSession* session) {
  syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  if (session->status_controller()->TestAndClearIsDirty()) {
    SyncEngineEvent event(SyncEngineEvent::STATUS_CHANGED);
    const sessions::SyncSessionSnapshot& snapshot(session->TakeSnapshot());
    event.snapshot = &snapshot;
    session->context()->NotifyListeners(event);
  }
}

}  // namespace browser_sync
