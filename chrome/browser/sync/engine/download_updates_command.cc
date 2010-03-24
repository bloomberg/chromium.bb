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
#include "chrome/browser/sync/util/sync_types.h"

using syncable::ScopedDirLookup;

namespace browser_sync {
using sessions::StatusController;
using sessions::SyncSession;
using std::string;

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
  LOG(INFO) << "Getting updates from ts " << dir->last_download_timestamp();
  get_updates->set_from_timestamp(dir->last_download_timestamp());

  // We want folders for our associated types, always.  If we were to set
  // this to false, the server would send just the non-container items
  // (e.g. Bookmark URLs but not their containing folders).
  get_updates->set_fetch_folders(true);

  // Set the requested_types protobuf field so that we fetch all enabled types.
  SetRequestedTypes(session->routing_info(),
                    get_updates->mutable_requested_types());

  // Set GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(session->TestAndSetSource());
  get_updates->mutable_caller_info()->set_notifications_enabled(
      session->context()->notifications_enabled());

  bool ok = SyncerProtoUtil::PostClientToServerMessage(
      &client_to_server_message,
      &update_response,
      session);

  DLOG(INFO) << SyncerProtoUtil::ClientToServerResponseDebugString(
      update_response);

  StatusController* status = session->status_controller();
  if (!ok) {
    status->increment_num_consecutive_errors();
    status->mutable_updates_response()->Clear();
    LOG(ERROR) << "PostClientToServerMessage() failed during GetUpdates";
    return;
  }
  status->mutable_updates_response()->CopyFrom(update_response);

  LOG(INFO) << "GetUpdates from ts " << get_updates->from_timestamp()
            << " returned " << update_response.get_updates().entries_size()
            << " updates.";
}

void DownloadUpdatesCommand::SetRequestedTypes(
    const ModelSafeRoutingInfo& routing_info,
    sync_pb::EntitySpecifics* types) {
  // The datatypes which should be synced are dictated by the value of the
  // ModelSafeRoutingInfo.  If a datatype is in the routing info map, it
  // should be synced (even if it's GROUP_PASSIVE).
  int requested_type_count = 0;
  for (ModelSafeRoutingInfo::const_iterator i = routing_info.begin();
       i != routing_info.end();
       ++i) {
    requested_type_count++;
    syncable::AddDefaultExtensionValue(i->first, types);
  }
  DCHECK_LT(0, requested_type_count) << "Doing GetUpdates with empty filter.";
}

}  // namespace browser_sync
