// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_proto_util.h"

#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/protocol/sync_protocol_error.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/time.h"

using browser_sync::SyncProtocolErrorType;
using std::string;
using std::stringstream;
using syncable::BASE_VERSION;
using syncable::CTIME;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNSYNCED;
using syncable::MTIME;
using syncable::PARENT_ID;
using syncable::ScopedDirLookup;

namespace browser_sync {
using sessions::SyncSession;

namespace {

// Time to backoff syncing after receiving a throttled response.
static const int kSyncDelayAfterThrottled = 2 * 60 * 60;  // 2 hours
void LogResponseProfilingData(const ClientToServerResponse& response) {
  if (response.has_profiling_data()) {
    stringstream response_trace;
    response_trace << "Server response trace:";

    if (response.profiling_data().has_user_lookup_time()) {
      response_trace << " user lookup: "
                     << response.profiling_data().user_lookup_time() << "ms";
    }

    if (response.profiling_data().has_meta_data_write_time()) {
      response_trace << " meta write: "
                     << response.profiling_data().meta_data_write_time()
                     << "ms";
    }

    if (response.profiling_data().has_meta_data_read_time()) {
      response_trace << " meta read: "
                     << response.profiling_data().meta_data_read_time() << "ms";
    }

    if (response.profiling_data().has_file_data_write_time()) {
      response_trace << " file write: "
                     << response.profiling_data().file_data_write_time()
                     << "ms";
    }

    if (response.profiling_data().has_file_data_read_time()) {
      response_trace << " file read: "
                     << response.profiling_data().file_data_read_time() << "ms";
    }

    if (response.profiling_data().has_total_request_time()) {
      response_trace << " total time: "
                     << response.profiling_data().total_request_time() << "ms";
    }
    DVLOG(1) << response_trace.str();
  }
}

}  // namespace

// static
void SyncerProtoUtil::HandleMigrationDoneResponse(
  const sync_pb::ClientToServerResponse* response,
  sessions::SyncSession* session) {
  LOG_IF(ERROR, 0 >= response->migrated_data_type_id_size())
      << "MIGRATION_DONE but no types specified.";
  syncable::ModelTypeSet to_migrate;
  for (int i = 0; i < response->migrated_data_type_id_size(); i++) {
    to_migrate.insert(syncable::GetModelTypeFromExtensionFieldNumber(
        response->migrated_data_type_id(i)));
  }
  // TODO(akalin): This should be a set union.
  session->mutable_status_controller()->
      set_types_needing_local_migration(to_migrate);
}

// static
bool SyncerProtoUtil::VerifyResponseBirthday(syncable::Directory* dir,
    const ClientToServerResponse* response) {

  std::string local_birthday = dir->store_birthday();

  if (local_birthday.empty()) {
    if (!response->has_store_birthday()) {
      LOG(WARNING) << "Expected a birthday on first sync.";
      return false;
    }

    DVLOG(1) << "New store birthday: " << response->store_birthday();
    dir->set_store_birthday(response->store_birthday());
    return true;
  }

  // Error situation, but we're not stuck.
  if (!response->has_store_birthday()) {
    LOG(WARNING) << "No birthday in server response?";
    return true;
  }

  if (response->store_birthday() != local_birthday) {
    LOG(WARNING) << "Birthday changed, showing syncer stuck";
    return false;
  }

  return true;
}

// static
void SyncerProtoUtil::AddRequestBirthday(syncable::Directory* dir,
                                         ClientToServerMessage* msg) {
  if (!dir->store_birthday().empty())
    msg->set_store_birthday(dir->store_birthday());
}

// static
bool SyncerProtoUtil::PostAndProcessHeaders(ServerConnectionManager* scm,
                                            sessions::SyncSession* session,
                                            const ClientToServerMessage& msg,
                                            ClientToServerResponse* response) {
  ServerConnectionManager::PostBufferParams params;
  msg.SerializeToString(&params.buffer_in);

  ScopedServerStatusWatcher server_status_watcher(scm, &params.response);
  // Fills in params.buffer_out and params.response.
  if (!scm->PostBufferWithCachedAuth(&params, &server_status_watcher)) {
    LOG(WARNING) << "Error posting from syncer:" << params.response;
    return false;
  }

  std::string new_token = params.response.update_client_auth_header;
  if (!new_token.empty()) {
    SyncEngineEvent event(SyncEngineEvent::UPDATED_TOKEN);
    event.updated_token = new_token;
    session->context()->NotifyListeners(event);
  }

  if (response->ParseFromString(params.buffer_out)) {
    // TODO(tim): This is an egregious layering violation (bug 35060).
    switch (response->error_code()) {
      case ClientToServerResponse::ACCESS_DENIED:
      case ClientToServerResponse::AUTH_INVALID:
      case ClientToServerResponse::USER_NOT_ACTIVATED:
        // Fires on ScopedServerStatusWatcher
        params.response.server_status = HttpResponse::SYNC_AUTH_ERROR;
        return false;
      default:
        return true;
    }
  }

  return false;
}

base::TimeDelta SyncerProtoUtil::GetThrottleDelay(
    const sync_pb::ClientToServerResponse& response) {
  base::TimeDelta throttle_delay =
      base::TimeDelta::FromSeconds(kSyncDelayAfterThrottled);
  if (response.has_client_command()) {
    const sync_pb::ClientCommand& command = response.client_command();
    if (command.has_throttle_delay_seconds()) {
      throttle_delay =
          base::TimeDelta::FromSeconds(command.throttle_delay_seconds());
    }
  }
  return throttle_delay;
}

void SyncerProtoUtil::HandleThrottleError(
    const SyncProtocolError& error,
    const base::TimeTicks& throttled_until,
    sessions::SyncSessionContext* context,
    sessions::SyncSession::Delegate* delegate) {
  DCHECK_EQ(error.error_type, browser_sync::THROTTLED);
  if (error.error_data_types.size() > 0) {
     context->SetUnthrottleTime(error.error_data_types, throttled_until);
  } else {
    // No datatypes indicates the client should be completely throttled.
    delegate->OnSilencedUntil(throttled_until);
  }
}

namespace {

// Helper function for an assertion in PostClientToServerMessage.
bool IsVeryFirstGetUpdates(const ClientToServerMessage& message) {
  if (!message.has_get_updates())
    return false;
  DCHECK_LT(0, message.get_updates().from_progress_marker_size());
  for (int i = 0; i < message.get_updates().from_progress_marker_size(); ++i) {
    if (!message.get_updates().from_progress_marker(i).token().empty())
      return false;
  }
  return true;
}

SyncProtocolErrorType ConvertSyncProtocolErrorTypePBToLocalType(
    const sync_pb::ClientToServerResponse::ErrorType& error_type) {
  switch (error_type) {
    case ClientToServerResponse::SUCCESS:
      return browser_sync::SYNC_SUCCESS;
    case ClientToServerResponse::NOT_MY_BIRTHDAY:
      return browser_sync::NOT_MY_BIRTHDAY;
    case ClientToServerResponse::THROTTLED:
      return browser_sync::THROTTLED;
    case ClientToServerResponse::CLEAR_PENDING:
      return browser_sync::CLEAR_PENDING;
    case ClientToServerResponse::TRANSIENT_ERROR:
      return browser_sync::TRANSIENT_ERROR;
    case ClientToServerResponse::MIGRATION_DONE:
      return browser_sync::MIGRATION_DONE;
    case ClientToServerResponse::UNKNOWN:
      return browser_sync::UNKNOWN_ERROR;
    case ClientToServerResponse::USER_NOT_ACTIVATED:
    case ClientToServerResponse::AUTH_INVALID:
    case ClientToServerResponse::ACCESS_DENIED:
      return browser_sync::INVALID_CREDENTIAL;
    default:
      NOTREACHED();
      return browser_sync::UNKNOWN_ERROR;
  }
}

browser_sync::ClientAction ConvertClientActionPBToLocalClientAction(
    const sync_pb::ClientToServerResponse::Error::Action& action) {
  switch (action) {
    case ClientToServerResponse::Error::UPGRADE_CLIENT:
      return browser_sync::UPGRADE_CLIENT;
    case ClientToServerResponse::Error::CLEAR_USER_DATA_AND_RESYNC:
      return browser_sync::CLEAR_USER_DATA_AND_RESYNC;
    case ClientToServerResponse::Error::ENABLE_SYNC_ON_ACCOUNT:
      return browser_sync::ENABLE_SYNC_ON_ACCOUNT;
    case ClientToServerResponse::Error::STOP_AND_RESTART_SYNC:
      return browser_sync::STOP_AND_RESTART_SYNC;
    case ClientToServerResponse::Error::DISABLE_SYNC_ON_CLIENT:
      return browser_sync::DISABLE_SYNC_ON_CLIENT;
    case ClientToServerResponse::Error::UNKNOWN_ACTION:
      return browser_sync::UNKNOWN_ACTION;
    default:
      NOTREACHED();
      return browser_sync::UNKNOWN_ACTION;
  }
}

browser_sync::SyncProtocolError ConvertErrorPBToLocalType(
    const sync_pb::ClientToServerResponse::Error& error) {
  browser_sync::SyncProtocolError sync_protocol_error;
  sync_protocol_error.error_type = ConvertSyncProtocolErrorTypePBToLocalType(
      error.error_type());
  sync_protocol_error.error_description = error.error_description();
  sync_protocol_error.url = error.url();
  sync_protocol_error.action = ConvertClientActionPBToLocalClientAction(
      error.action());

  if (error.error_data_type_ids_size() > 0) {
    // THROTTLED is currently the only error code that uses |error_data_types|.
    DCHECK_EQ(error.error_type(), ClientToServerResponse::THROTTLED);
    for (int i = 0; i < error.error_data_type_ids_size(); ++i) {
      sync_protocol_error.error_data_types.insert(
          syncable::GetModelTypeFromExtensionFieldNumber(
              error.error_data_type_ids(i)));
    }
  }

  return sync_protocol_error;
}

// TODO(lipalani) : Rename these function names as per the CR for issue 7740067.
browser_sync::SyncProtocolError ConvertLegacyErrorCodeToNewError(
    const sync_pb::ClientToServerResponse::ErrorType& error_type) {
  browser_sync::SyncProtocolError error;
  error.error_type = ConvertSyncProtocolErrorTypePBToLocalType(error_type);
  if (error_type == ClientToServerResponse::CLEAR_PENDING ||
      error_type == ClientToServerResponse::NOT_MY_BIRTHDAY) {
      error.action = browser_sync::DISABLE_SYNC_ON_CLIENT;
  }  // There is no other action we can compute for legacy server.
  return error;
}

}  // namespace

// static
bool SyncerProtoUtil::PostClientToServerMessage(
    const ClientToServerMessage& msg,
    ClientToServerResponse* response,
    SyncSession* session) {

  CHECK(response);
  DCHECK(!msg.get_updates().has_from_timestamp());  // Deprecated.
  DCHECK(!msg.get_updates().has_requested_types());  // Deprecated.
  DCHECK(msg.has_store_birthday() || IsVeryFirstGetUpdates(msg))
      << "Must call AddRequestBirthday to set birthday.";

  ScopedDirLookup dir(session->context()->directory_manager(),
      session->context()->account_name());
  if (!dir.good())
    return false;

  if (!PostAndProcessHeaders(session->context()->connection_manager(), session,
                             msg, response))
    return false;

  browser_sync::SyncProtocolError sync_protocol_error;

  // Birthday mismatch overrides any error that is sent by the server.
  if (!VerifyResponseBirthday(dir, response)) {
    sync_protocol_error.error_type = browser_sync::NOT_MY_BIRTHDAY;
     sync_protocol_error.action =
         browser_sync::DISABLE_SYNC_ON_CLIENT;
  } else if (response->has_error()) {
    // This is a new server. Just get the error from the protocol.
    sync_protocol_error = ConvertErrorPBToLocalType(response->error());
  } else {
    // Legacy server implementation. Compute the error based on |error_code|.
    sync_protocol_error = ConvertLegacyErrorCodeToNewError(
        response->error_code());
  }

  // Now set the error into the status so the layers above us could read it.
  sessions::StatusController* status = session->mutable_status_controller();
  status->set_sync_protocol_error(sync_protocol_error);

  // Inform the delegate of the error we got.
  session->delegate()->OnSyncProtocolError(session->TakeSnapshot());

  // Now do any special handling for the error type and decide on the return
  // value.
  switch (sync_protocol_error.error_type) {
    case browser_sync::UNKNOWN_ERROR:
      LOG(WARNING) << "Sync protocol out-of-date. The server is using a more "
                   << "recent version.";
      return false;
    case browser_sync::SYNC_SUCCESS:
      LogResponseProfilingData(*response);
      return true;
    case browser_sync::THROTTLED:
      LOG(WARNING) << "Client silenced by server.";
      HandleThrottleError(sync_protocol_error,
                          base::TimeTicks::Now() + GetThrottleDelay(*response),
                          session->context(),
                          session->delegate());
      return false;
    case browser_sync::TRANSIENT_ERROR:
      return false;
    case browser_sync::MIGRATION_DONE:
      HandleMigrationDoneResponse(response, session);
      return false;
    case browser_sync::CLEAR_PENDING:
    case browser_sync::NOT_MY_BIRTHDAY:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

// static
bool SyncerProtoUtil::Compare(const syncable::Entry& local_entry,
                              const SyncEntity& server_entry) {
  const std::string name = NameFromSyncEntity(server_entry);

  CHECK(local_entry.Get(ID) == server_entry.id()) <<
      " SyncerProtoUtil::Compare precondition not met.";
  CHECK(server_entry.version() == local_entry.Get(BASE_VERSION)) <<
      " SyncerProtoUtil::Compare precondition not met.";
  CHECK(!local_entry.Get(IS_UNSYNCED)) <<
      " SyncerProtoUtil::Compare precondition not met.";

  if (local_entry.Get(IS_DEL) && server_entry.deleted())
    return true;
  if (local_entry.Get(CTIME) != ProtoTimeToTime(server_entry.ctime())) {
    LOG(WARNING) << "ctime mismatch";
    return false;
  }

  // These checks are somewhat prolix, but they're easier to debug than a big
  // boolean statement.
  string client_name = local_entry.Get(syncable::NON_UNIQUE_NAME);
  if (client_name != name) {
    LOG(WARNING) << "Client name mismatch";
    return false;
  }
  if (local_entry.Get(PARENT_ID) != server_entry.parent_id()) {
    LOG(WARNING) << "Parent ID mismatch";
    return false;
  }
  if (local_entry.Get(IS_DIR) != server_entry.IsFolder()) {
    LOG(WARNING) << "Dir field mismatch";
    return false;
  }
  if (local_entry.Get(IS_DEL) != server_entry.deleted()) {
    LOG(WARNING) << "Deletion mismatch";
    return false;
  }
  if (!local_entry.Get(IS_DIR) &&
      (local_entry.Get(MTIME) != ProtoTimeToTime(server_entry.mtime()))) {
    LOG(WARNING) << "mtime mismatch";
    return false;
  }

  return true;
}

// static
void SyncerProtoUtil::CopyProtoBytesIntoBlob(const std::string& proto_bytes,
                                             syncable::Blob* blob) {
  syncable::Blob proto_blob(proto_bytes.begin(), proto_bytes.end());
  blob->swap(proto_blob);
}

// static
bool SyncerProtoUtil::ProtoBytesEqualsBlob(const std::string& proto_bytes,
                                           const syncable::Blob& blob) {
  if (proto_bytes.size() != blob.size())
    return false;
  return std::equal(proto_bytes.begin(), proto_bytes.end(), blob.begin());
}

// static
void SyncerProtoUtil::CopyBlobIntoProtoBytes(const syncable::Blob& blob,
                                             std::string* proto_bytes) {
  std::string blob_string(blob.begin(), blob.end());
  proto_bytes->swap(blob_string);
}

// static
const std::string& SyncerProtoUtil::NameFromSyncEntity(
    const sync_pb::SyncEntity& entry) {
  if (entry.has_non_unique_name())
    return entry.non_unique_name();
  return entry.name();
}

// static
const std::string& SyncerProtoUtil::NameFromCommitEntryResponse(
    const CommitResponse_EntryResponse& entry) {
  if (entry.has_non_unique_name())
    return entry.non_unique_name();
  return entry.name();
}

std::string SyncerProtoUtil::SyncEntityDebugString(
    const sync_pb::SyncEntity& entry) {
  const std::string& mtime_str =
      GetTimeDebugString(ProtoTimeToTime(entry.mtime()));
  const std::string& ctime_str =
      GetTimeDebugString(ProtoTimeToTime(entry.ctime()));
  return base::StringPrintf(
      "id: %s, parent_id: %s, "
      "version: %"PRId64"d, "
      "mtime: %" PRId64"d (%s), "
      "ctime: %" PRId64"d (%s), "
      "name: %s, sync_timestamp: %" PRId64"d, "
      "%s ",
      entry.id_string().c_str(),
      entry.parent_id_string().c_str(),
      entry.version(),
      entry.mtime(), mtime_str.c_str(),
      entry.ctime(), ctime_str.c_str(),
      entry.name().c_str(), entry.sync_timestamp(),
      entry.deleted() ? "deleted, ":"");
}

namespace {
std::string GetUpdatesResponseString(
    const sync_pb::GetUpdatesResponse& response) {
  std::string output;
  output.append("GetUpdatesResponse:\n");
  for (int i = 0; i < response.entries_size(); i++) {
    output.append(SyncerProtoUtil::SyncEntityDebugString(response.entries(i)));
    output.append("\n");
  }
  return output;
}
}  // namespace

std::string SyncerProtoUtil::ClientToServerResponseDebugString(
    const sync_pb::ClientToServerResponse& response) {
  // Add more handlers as needed.
  std::string output;
  if (response.has_get_updates())
    output.append(GetUpdatesResponseString(response.get_updates()));
  return output;
}

}  // namespace browser_sync
