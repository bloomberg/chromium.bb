// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_command.h"

#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/sync_types.h"

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
    SyncerEvent event(SyncerEvent::STATUS_CHANGED);
    const sessions::SyncSessionSnapshot& snapshot(session->TakeSnapshot());
    event.snapshot = &snapshot;
    DCHECK(session->context()->syncer_event_channel());
    session->context()->syncer_event_channel()->NotifyListeners(event);
    if (session->status_controller()->syncer_status().over_quota) {
      SyncerEvent quota_event(SyncerEvent::OVER_QUOTA);
      quota_event.snapshot = &snapshot;
      session->context()->syncer_event_channel()->NotifyListeners(quota_event);
    }
  }
  if (session->auth_failure_occurred()) {
    ServerConnectionEvent event;
    event.what_happened = ServerConnectionEvent::STATUS_CHANGED;
    event.server_reachable = true;
    event.connection_code = HttpResponse::SYNC_AUTH_ERROR;
    session->context()->connection_manager()->channel()->NotifyListeners(event);
    session->clear_auth_failure_occurred();
  }
}

}  // namespace browser_sync
