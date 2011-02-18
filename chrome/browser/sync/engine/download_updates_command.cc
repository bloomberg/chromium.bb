// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/download_updates_command.h"

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

DownloadUpdatesCommand::DownloadUpdatesCommand() {}
DownloadUpdatesCommand::~DownloadUpdatesCommand() {}

void DownloadUpdatesCommand::ExecuteImpl(SyncSession* session) {
  ClientToServerMessage client_to_server_message;
  ClientToServerResponse update_response;

  client_to_server_message.set_share(session->context()->account_name());
  client_to_server_message.set_message_contents(
      ClientToServerMessage::GET_UPDATES);
  GetUpdatesMessage* get_updates =
      client_to_server_message.mutable_get_updates();

  ScopedDirLookup dir(session->context()->directory_manager(),
                      session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  // Request updates for all enabled types.
  syncable::ModelTypeBitSet enabled_types;
  for (ModelSafeRoutingInfo::const_iterator i = session->routing_info().begin();
       i != session->routing_info().end(); ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i->first);
    enabled_types[i->first] = true;
    dir->GetDownloadProgress(model_type,
        get_updates->add_from_progress_marker());
  }

  VLOG(1) << "Getting updates for types " << enabled_types.to_string();
  DCHECK(enabled_types.any());

  // We want folders for our associated types, always.  If we were to set
  // this to false, the server would send just the non-container items
  // (e.g. Bookmark URLs but not their containing folders).
  get_updates->set_fetch_folders(true);

  // Set GetUpdatesMessage.GetUpdatesCallerInfo information.
  // TODO(zea): send SyncSourceInfo's payloads to server once we know it's all
  // working properly.
  get_updates->mutable_caller_info()->set_source(
      session->TestAndSetSource().updates_source);
  get_updates->mutable_caller_info()->set_notifications_enabled(
      session->context()->notifications_enabled());

  SyncerProtoUtil::AddRequestBirthday(dir, &client_to_server_message);

  bool ok = SyncerProtoUtil::PostClientToServerMessage(
      client_to_server_message,
      &update_response,
      session);

  VLOG(2) << SyncerProtoUtil::ClientToServerResponseDebugString(
      update_response);

  StatusController* status = session->status_controller();
  status->set_updates_request_types(enabled_types);
  if (!ok) {
    status->increment_num_consecutive_errors();
    status->mutable_updates_response()->Clear();
    LOG(ERROR) << "PostClientToServerMessage() failed during GetUpdates";
    return;
  }

  status->mutable_updates_response()->CopyFrom(update_response);

  VLOG(1) << "GetUpdates "
          << " returned " << update_response.get_updates().entries_size()
          << " updates and indicated "
          << update_response.get_updates().changes_remaining()
          << " updates left on server.";
}

void DownloadUpdatesCommand::SetRequestedTypes(
    const syncable::ModelTypeBitSet& target_datatypes,
    sync_pb::EntitySpecifics* filter_protobuf) {
  // The datatypes which should be synced are dictated by the value of the
  // ModelSafeRoutingInfo.  If a datatype is in the routing info map, it
  // should be synced (even if it's GROUP_PASSIVE).
  int requested_type_count = 0;
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    if (target_datatypes[i]) {
      requested_type_count++;
      syncable::AddDefaultExtensionValue(syncable::ModelTypeFromInt(i),
                                         filter_protobuf);
    }
  }
  DCHECK_LT(0, requested_type_count) << "Doing GetUpdates with empty filter.";
}

}  // namespace browser_sync
