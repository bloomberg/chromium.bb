// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_command.h"

#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncer_status.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/sync_types.h"

namespace browser_sync {

SyncerCommand::SyncerCommand() {}
SyncerCommand::~SyncerCommand() {}

void SyncerCommand::Execute(SyncerSession* session) {
  ExecuteImpl(session);
  SendNotifications(session);
}

void SyncerCommand::SendNotifications(SyncerSession* session) {
  syncable::ScopedDirLookup dir(session->dirman(), session->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  SyncerStatus status(session);

  if (status.IsDirty()) {
    SyncerEvent event = { SyncerEvent::STATUS_CHANGED};
    event.last_session = session;
    session->syncer_event_channel()->NotifyListeners(event);
    if (status.over_quota()) {
      SyncerEvent quota_event = {SyncerEvent::OVER_QUOTA};
      quota_event.last_session = session;
      session->syncer_event_channel()->NotifyListeners(quota_event);
    }
    status.SetClean();
  }
  if (status.IsAuthDirty()) {
    ServerConnectionEvent event;
    event.what_happened = ServerConnectionEvent::STATUS_CHANGED;
    event.server_reachable = true;
    event.connection_code = HttpResponse::SYNC_AUTH_ERROR;
    session->connection_manager()->channel()->NotifyListeners(event);
    status.SetAuthClean();
  }
}

}  // namespace browser_sync
