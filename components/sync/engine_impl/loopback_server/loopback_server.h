// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_LOOPBACK_SERVER_LOOPBACK_SERVER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_LOOPBACK_SERVER_LOOPBACK_SERVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine_impl/loopback_server/loopback_server_entity.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// A loopback version of the Sync server used for local profile serialization.
class LoopbackServer {
 public:
  explicit LoopbackServer(const base::FilePath& persistent_file);
  virtual ~LoopbackServer();

  // Handles a /command POST (with the given |request|) to the server. Three
  // output arguments, |server_status|, |response_code|, and |response|, are
  // used to pass data back to the caller. The command has failed if the value
  // pointed to by |error_code| is nonzero.
  void HandleCommand(const std::string& request,
                     HttpResponse::ServerConnectionCode* server_status,
                     int64_t* response_code,
                     std::string* response);

 private:
  using EntityMap =
      std::map<std::string, std::unique_ptr<LoopbackServerEntity>>;

  // Gets LoopbackServer ready for syncing.
  void Init();

  // Processes a GetUpdates call.
  bool HandleGetUpdatesRequest(const sync_pb::GetUpdatesMessage& get_updates,
                               sync_pb::GetUpdatesResponse* response);

  // Processes a Commit call.
  bool HandleCommitRequest(const sync_pb::CommitMessage& message,
                           const std::string& invalidator_client_id,
                           sync_pb::CommitResponse* response);

  void ClearServerData();

  // Creates and saves a permanent folder for Bookmarks (e.g., Bookmark Bar).
  bool CreatePermanentBookmarkFolder(const std::string& server_tag,
                                     const std::string& name);

  // Inserts the default permanent items in |entities_|.
  bool CreateDefaultPermanentItems();

  std::string GenerateNewKeystoreKey() const;

  // Saves a |entity| to |entities_|.
  void SaveEntity(std::unique_ptr<LoopbackServerEntity> entity);

  // Commits a client-side SyncEntity to the server as a LoopbackServerEntity.
  // The client that sent the commit is identified via |client_guid|. The
  // parent ID string present in |client_entity| should be ignored in favor
  // of |parent_id|. If the commit is successful, the entity's server ID string
  // is returned and a new LoopbackServerEntity is saved in |entities_|.
  std::string CommitEntity(
      const sync_pb::SyncEntity& client_entity,
      sync_pb::CommitResponse_EntryResponse* entry_response,
      const std::string& client_guid,
      const std::string& parent_id);

  // Populates |entry_response| based on the stored entity identified by
  // |entity_id|. It is assumed that the entity identified by |entity_id| has
  // already been stored using SaveEntity.
  void BuildEntryResponseForSuccessfulCommit(
      const std::string& entity_id,
      sync_pb::CommitResponse_EntryResponse* entry_response);

  // Determines whether the SyncEntity with id_string |id| is a child of an
  // entity with id_string |potential_parent_id|.
  bool IsChild(const std::string& id, const std::string& potential_parent_id);

  // Creates and saves tombstones for all children of the entity with the given
  // |id|. A tombstone is not created for the entity itself.
  void DeleteChildren(const std::string& id);

  // Updates the |entity| to a new version and increments the version counter
  // that the server uses to assign versions.
  void UpdateEntityVersion(LoopbackServerEntity* entity);

  // Returns the store birthday.
  std::string GetStoreBirthday() const;

  // Serializes the server state to |proto|.
  void SerializeState(sync_pb::LoopbackServerProto* proto) const;

  // Populates the server state from |proto|. Returns true iff successful.
  bool DeSerializeState(const sync_pb::LoopbackServerProto& proto);

  // Saves all entities and server state to a protobuf file in |filename|.
  bool SaveStateToFile(const base::FilePath& filename) const;

  // Loads all entities and server state from a protobuf file in |filename|.
  bool LoadStateFromFile(const base::FilePath& filename);

  // This is the last version number assigned to an entity. The next entity will
  // have a version number of version_ + 1.
  int64_t version_;

  int64_t store_birthday_;

  EntityMap entities_;
  std::vector<std::string> keystore_keys_;

  // The file used to store the local sync data.
  base::FilePath persistent_file_;

  // Used to verify that LoopbackServer is only used from one thread.
  base::ThreadChecker thread_checker_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_LOOPBACK_SERVER_LOOPBACK_SERVER_H_
