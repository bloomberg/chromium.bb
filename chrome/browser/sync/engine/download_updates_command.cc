// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/download_updates_command.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/common/chrome_switches.h"

using syncable::ScopedDirLookup;

using sync_pb::DebugInfo;

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
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSyncedBookmarksFolder)) {
    get_updates->set_include_syncable_bookmarks(true);
  }

  ScopedDirLookup dir(session->context()->directory_manager(),
                      session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  // Request updates for all enabled types.
  syncable::ModelTypeBitSet enabled_types;
  const syncable::ModelTypePayloadMap& type_payload_map =
      session->source().types;
  for (ModelSafeRoutingInfo::const_iterator i = session->routing_info().begin();
       i != session->routing_info().end(); ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i->first);
    enabled_types[i->first] = true;
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->add_from_progress_marker();
    dir->GetDownloadProgress(model_type, progress_marker);

    // Set notification hint if present.
    syncable::ModelTypePayloadMap::const_iterator type_payload =
        type_payload_map.find(i->first);
    if (type_payload != type_payload_map.end()) {
      progress_marker->set_notification_hint(type_payload->second);
    }
  }

  VLOG(1) << "Getting updates for types " << enabled_types.to_string();
  DCHECK(enabled_types.any());

  // We want folders for our associated types, always.  If we were to set
  // this to false, the server would send just the non-container items
  // (e.g. Bookmark URLs but not their containing folders).
  get_updates->set_fetch_folders(true);

  // Set GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(
      session->TestAndSetSource().updates_source);
  get_updates->mutable_caller_info()->set_notifications_enabled(
      session->context()->notifications_enabled());

  SyncerProtoUtil::AddRequestBirthday(dir, &client_to_server_message);

  DebugInfo* debug_info = client_to_server_message.mutable_debug_info();

  AppendClientDebugInfoIfNeeded(session, debug_info);

  bool ok = SyncerProtoUtil::PostClientToServerMessage(
      client_to_server_message,
      &update_response,
      session);

  VLOG(2) << SyncerProtoUtil::ClientToServerResponseDebugString(
      update_response);

  StatusController* status = session->mutable_status_controller();
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

void DownloadUpdatesCommand::AppendClientDebugInfoIfNeeded(
    sessions::SyncSession* session,
    DebugInfo* debug_info) {
  // We want to send the debug info only once per sync cycle. Check if it has
  // already been sent.
  if (!session->status_controller().debug_info_sent()) {
    VLOG(1) << "Sending client debug info ...";
    // could be null in some unit tests.
    if (session->context()->debug_info_getter()) {
      session->context()->debug_info_getter()->GetAndClearDebugInfo(
          debug_info);
    }
    session->mutable_status_controller()->set_debug_info_sent();
  }
}


}  // namespace browser_sync
