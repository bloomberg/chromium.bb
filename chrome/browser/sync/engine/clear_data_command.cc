// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/clear_data_command.h"

#include <string>

#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"

using syncable::ScopedDirLookup;

namespace browser_sync {
using sessions::StatusController;
using sessions::SyncSession;
using std::string;
using syncable::FIRST_REAL_MODEL_TYPE;
using syncable::MODEL_TYPE_COUNT;

ClearDataCommand::ClearDataCommand() {}
ClearDataCommand::~ClearDataCommand() {}

void ClearDataCommand::ExecuteImpl(SyncSession* session) {
  ClientToServerMessage client_to_server_message;
  ClientToServerResponse client_to_server_response;

  client_to_server_message.set_share(session->context()->account_name());
  client_to_server_message.set_message_contents(
      ClientToServerMessage::CLEAR_DATA);

  client_to_server_message.mutable_clear_user_data();

  ScopedDirLookup dir(session->context()->directory_manager(),
                      session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  SyncerProtoUtil::AddRequestBirthday(dir, &client_to_server_message);

  LOG(INFO) << "Clearing server data";

  bool ok = SyncerProtoUtil::PostClientToServerMessage(
      client_to_server_message,
      &client_to_server_response,
      session);

  DLOG(INFO) << SyncerProtoUtil::ClientToServerResponseDebugString(
      client_to_server_response);

  if (!ok || !client_to_server_response.has_clear_user_data() ||
      !client_to_server_response.clear_user_data().has_status() ||
      client_to_server_response.clear_user_data().status() !=
      sync_pb::SUCCESS) {
    // On failure, subsequent requests to the server will cause it to attempt
    // to resume the clear.  The client will handle disabling of sync in
    // response to a store birthday error from the server.
    SyncerEvent event(SyncerEvent::CLEAR_SERVER_DATA_FAILED);
    session->context()->syncer_event_channel()->Notify(event);

    LOG(ERROR) << "Error posting ClearData.";

    return;
  }

  SyncerEvent event(SyncerEvent::CLEAR_SERVER_DATA_SUCCEEDED);
  session->context()->syncer_event_channel()->Notify(event);

  session->delegate()->OnShouldStopSyncingPermanently();

  LOG(INFO) << "ClearData succeeded.";
}

}  // namespace browser_sync

