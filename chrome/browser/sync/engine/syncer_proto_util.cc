// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_proto_util.h"

#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable.h"

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
using syncable::SyncName;

namespace browser_sync {
using sessions::SyncSession;

namespace {

// Time to backoff syncing after receiving a throttled response.
static const int kSyncDelayAfterThrottled = 2 * 60 * 60;  // 2 hours

// Verifies the store birthday, alerting/resetting as appropriate if there's a
// mismatch.
bool VerifyResponseBirthday(const ScopedDirLookup& dir,
                            const ClientToServerResponse* response) {
  // Process store birthday.
  if (!response->has_store_birthday())
    return true;
  string birthday = dir->store_birthday();
  if (response->store_birthday() == birthday)
    return true;
  LOG(INFO) << "New store birthday: " << response->store_birthday();
  if (!birthday.empty()) {
    LOG(ERROR) << "Birthday changed, showing syncer stuck";
    return false;
  }
  dir->set_store_birthday(response->store_birthday());
  return true;
}

void LogResponseProfilingData(const ClientToServerResponse& response) {
  if (response.has_profiling_data()) {
    stringstream response_trace;
    response_trace << "Server response trace:";

    if (response.profiling_data().has_user_lookup_time()) {
      response_trace << " " << "user lookup: " <<
          response.profiling_data().user_lookup_time() << "ms";
    }

    if (response.profiling_data().has_meta_data_write_time()) {
      response_trace << " " << "meta write: " <<
          response.profiling_data().meta_data_write_time() << "ms";
    }

    if (response.profiling_data().has_meta_data_read_time()) {
      response_trace << " " << "meta read: " <<
          response.profiling_data().meta_data_read_time() << "ms";
    }

    if (response.profiling_data().has_file_data_write_time()) {
      response_trace << " " << "file write: " <<
          response.profiling_data().file_data_write_time() << "ms";
    }

    if (response.profiling_data().has_file_data_read_time()) {
      response_trace << " " << "file read: " <<
          response.profiling_data().file_data_read_time() << "ms";
    }

    if (response.profiling_data().has_total_request_time()) {
      response_trace << " " << "total time: " <<
          response.profiling_data().total_request_time() << "ms";
    }
    LOG(INFO) << response_trace.str();
  }
}

}  // namespace

// static
bool SyncerProtoUtil::PostClientToServerMessage(ClientToServerMessage* msg,
    ClientToServerResponse* response, SyncSession* session) {
  bool rv = false;
  string tx, rx;
  CHECK(response);

  ScopedDirLookup dir(session->context()->directory_manager(),
      session->context()->account_name());
  if (!dir.good())
    return false;
  string birthday = dir->store_birthday();
  if (!birthday.empty()) {
    msg->set_store_birthday(birthday);
  } else {
    LOG(INFO) << "no birthday set";
  }

  msg->SerializeToString(&tx);
  HttpResponse http_response;
  ServerConnectionManager::PostBufferParams params = {
    tx, &rx, &http_response
  };

  ServerConnectionManager* scm = session->context()->connection_manager();
  if (!scm->PostBufferWithCachedAuth(&params)) {
    LOG(WARNING) << "Error posting from syncer:" << http_response;
  } else {
    rv = response->ParseFromString(rx);
  }
  if (rv) {
    if (!VerifyResponseBirthday(dir, response)) {
      // TODO(ncarter): Add a unit test for the case where the syncer becomes
      // stuck due to a bad birthday.
      session->status_controller()->set_syncer_stuck(true);
      return false;
    }

    switch (response->error_code()) {
      case ClientToServerResponse::SUCCESS:
        if (!response->has_store_birthday() && birthday.empty()) {
          LOG(ERROR) <<
            "Server didn't provide birthday in proto buffer response.";
          rv = false;
        }
        LogResponseProfilingData(*response);
        break;
      case ClientToServerResponse::USER_NOT_ACTIVATED:
      case ClientToServerResponse::AUTH_INVALID:
      case ClientToServerResponse::ACCESS_DENIED:
        LOG(INFO) << "Authentication expired, re-requesting";
        LOG(INFO) << "Not implemented in syncer yet!!!";
        session->set_auth_failure_occurred();
        rv = false;
        break;
      case ClientToServerResponse::NOT_MY_BIRTHDAY:
        LOG(WARNING) << "Not my birthday return.";
        rv = false;
        break;
      case ClientToServerResponse::THROTTLED:
        LOG(WARNING) << "Client silenced by server.";
        session->delegate()->OnSilencedUntil(base::TimeTicks::Now() +
            base::TimeDelta::FromSeconds(kSyncDelayAfterThrottled));
        rv = false;
        break;
      default:
        NOTREACHED();
        break;
    }

  }
  return rv;
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
  if (!ClientAndServerTimeMatch(local_entry.Get(CTIME), server_entry.ctime())) {
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
      !ClientAndServerTimeMatch(local_entry.Get(MTIME),
                                server_entry.mtime())) {
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
std::string SyncerProtoUtil::NameFromSyncEntity(
    const SyncEntity& entry) {

  if (entry.has_non_unique_name()) {
      return entry.non_unique_name();
  }

  return entry.name();
}

// static
std::string SyncerProtoUtil::NameFromCommitEntryResponse(
    const CommitResponse_EntryResponse& entry) {

  if (entry.has_non_unique_name()) {
      return entry.non_unique_name();
  }

  return entry.name();
}

}  // namespace browser_sync
