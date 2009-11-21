// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_PROTO_UTIL_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_PROTO_UTIL_H_

#include <string>

#include "chrome/browser/sync/syncable/blob.h"
#include "chrome/browser/sync/util/sync_types.h"

namespace syncable {
class Entry;
class ScopedDirLookup;
class SyncName;
}  // namespace syncable

namespace sync_pb {
class ClientToServerResponse;
}  // namespace sync_pb

namespace browser_sync {

namespace sessions {
class SyncSession;
}

class ClientToServerMessage;
class SyncEntity;
class CommitResponse_EntryResponse;

class SyncerProtoUtil {
 public:
  // Posts the given message and fills the buffer with the returned value.
  // Returns true on success.  Also handles store birthday verification:
  // session->status()->syncer_stuck_ is set true if the birthday is
  // incorrect.  A false value will always be returned if birthday is bad.
  static bool PostClientToServerMessage(ClientToServerMessage* msg,
      sync_pb::ClientToServerResponse* response,
      sessions::SyncSession* session);

  // Compares a syncable Entry to SyncEntity, returns true iff the data is
  // identical.
  //
  // TODO(sync): The places where this function is used are arguable big causes
  // of the fragility, because there's a tendency to freak out the moment the
  // local and server values diverge. However, this almost always indicates a
  // sync bug somewhere earlier in the sync cycle.
  static bool Compare(const syncable::Entry& local_entry,
                      const SyncEntity& server_entry);

  // Utility methods for converting between syncable::Blobs and protobuf byte
  // fields.
  static void CopyProtoBytesIntoBlob(const std::string& proto_bytes,
                                     syncable::Blob* blob);
  static bool ProtoBytesEqualsBlob(const std::string& proto_bytes,
                                   const syncable::Blob& blob);
  static void CopyBlobIntoProtoBytes(const syncable::Blob& blob,
                                     std::string* proto_bytes);

  // Extract the name field from a sync entity.
  static std::string NameFromSyncEntity(const SyncEntity& entry);


  // Extract the name field from a commit entry response.
  static std::string NameFromCommitEntryResponse(
      const CommitResponse_EntryResponse& entry);

 private:
  SyncerProtoUtil() {}
  DISALLOW_COPY_AND_ASSIGN(SyncerProtoUtil);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_PROTO_UTIL_H_
