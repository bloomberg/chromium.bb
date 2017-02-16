// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/loopback_server.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "components/sync/engine_impl/loopback_server/persistent_bookmark_entity.h"
#include "components/sync/engine_impl/loopback_server/persistent_permanent_entity.h"
#include "components/sync/engine_impl/loopback_server/persistent_tombstone_entity.h"
#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

using std::string;
using std::vector;

using syncer::GetModelType;
using syncer::GetModelTypeFromSpecifics;
using syncer::ModelType;
using syncer::ModelTypeSet;

namespace syncer {

class LoopbackServerEntity;

namespace {

static const int kCurrentLoopbackServerProtoVersion = 1;
static const int kKeystoreKeyLenght = 16;

// Properties of the bookmark bar permanent folders.
static const char kBookmarkBarFolderServerTag[] = "bookmark_bar";
static const char kBookmarkBarFolderName[] = "Bookmark Bar";
static const char kOtherBookmarksFolderServerTag[] = "other_bookmarks";
static const char kOtherBookmarksFolderName[] = "Other Bookmarks";
static const char kSyncedBookmarksFolderServerTag[] = "synced_bookmarks";
static const char kSyncedBookmarksFolderName[] = "Synced Bookmarks";

// A filter used during GetUpdates calls to determine what information to
// send back to the client. There is a 1:1 correspondence between any given
// GetUpdates call and an UpdateSieve instance.
class UpdateSieve {
 public:
  ~UpdateSieve() {}

  // Factory method for creating an UpdateSieve.
  static std::unique_ptr<UpdateSieve> Create(
      const sync_pb::GetUpdatesMessage& get_updates_message);

  // Sets the progress markers in |get_updates_response| given the progress
  // markers from the original GetUpdatesMessage and |new_version| (the latest
  // version in the entries sent back).
  void UpdateProgressMarkers(
      int64_t new_version,
      sync_pb::GetUpdatesResponse* get_updates_response) const {
    ModelTypeToVersionMap::const_iterator it;
    for (it = request_from_version_.begin(); it != request_from_version_.end();
         ++it) {
      sync_pb::DataTypeProgressMarker* new_marker =
          get_updates_response->add_new_progress_marker();
      new_marker->set_data_type_id(
          GetSpecificsFieldNumberFromModelType(it->first));

      int64_t version = std::max(new_version, it->second);
      new_marker->set_token(base::Int64ToString(version));
    }
  }

  // Determines whether the server should send an |entity| to the client as
  // part of a GetUpdatesResponse.
  bool ClientWantsItem(const LoopbackServerEntity& entity) const {
    int64_t version = entity.GetVersion();
    if (version <= min_version_) {
      return false;
    } else if (entity.IsDeleted()) {
      return true;
    }

    ModelTypeToVersionMap::const_iterator it =
        request_from_version_.find(entity.GetModelType());

    return it == request_from_version_.end() ? false : it->second < version;
  }

  // Returns the minimum version seen across all types.
  int64_t GetMinVersion() const { return min_version_; }

 private:
  using ModelTypeToVersionMap = std::map<ModelType, int64_t>;

  // Creates an UpdateSieve.
  UpdateSieve(const ModelTypeToVersionMap request_from_version,
              const int64_t min_version)
      : request_from_version_(request_from_version),
        min_version_(min_version) {}

  // Maps data type IDs to the latest version seen for that type.
  const ModelTypeToVersionMap request_from_version_;

  // The minimum version seen among all data types.
  const int min_version_;
};

std::unique_ptr<UpdateSieve> UpdateSieve::Create(
    const sync_pb::GetUpdatesMessage& get_updates_message) {
  CHECK_GT(get_updates_message.from_progress_marker_size(), 0)
      << "A GetUpdates request must have at least one progress marker.";

  UpdateSieve::ModelTypeToVersionMap request_from_version;
  int64_t min_version = std::numeric_limits<int64_t>::max();
  for (int i = 0; i < get_updates_message.from_progress_marker_size(); i++) {
    sync_pb::DataTypeProgressMarker marker =
        get_updates_message.from_progress_marker(i);

    int64_t version = 0;
    // Let the version remain zero if there is no token or an empty token (the
    // first request for this type).
    if (marker.has_token() && !marker.token().empty()) {
      bool parsed = base::StringToInt64(marker.token(), &version);
      CHECK(parsed) << "Unable to parse progress marker token.";
    }
    ModelType model_type =
        syncer::GetModelTypeFromSpecificsFieldNumber(marker.data_type_id());
    request_from_version[model_type] = version;

    if (version < min_version)
      min_version = version;
  }

  return std::unique_ptr<UpdateSieve>(
      new UpdateSieve(request_from_version, min_version));
}

}  // namespace

LoopbackServer::LoopbackServer(const base::FilePath& persistent_file)
    : version_(0), store_birthday_(0), persistent_file_(persistent_file) {
  Init();
}

LoopbackServer::~LoopbackServer() {}

void LoopbackServer::Init() {
  if (LoadStateFromFile(persistent_file_))
    return;

  keystore_keys_.push_back(GenerateNewKeystoreKey());

  const bool create_result = CreateDefaultPermanentItems();
  DCHECK(create_result) << "Permanent items were not created successfully.";
}

std::string LoopbackServer::GenerateNewKeystoreKey() const {
  // TODO(pastarmovj): Check if true random bytes is ok or alpha-nums is needed?
  return base::RandBytesAsString(kKeystoreKeyLenght);
}

bool LoopbackServer::CreatePermanentBookmarkFolder(
    const std::string& server_tag,
    const std::string& name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<LoopbackServerEntity> entity =
      PersistentPermanentEntity::Create(syncer::BOOKMARKS, server_tag, name,
                                        ModelTypeToRootTag(syncer::BOOKMARKS));
  if (!entity)
    return false;

  SaveEntity(std::move(entity));
  return true;
}

bool LoopbackServer::CreateDefaultPermanentItems() {
  // Permanent folders are always required for Bookmarks (hierarchical
  // structure) and Nigori (data stored in permanent root folder).
  ModelTypeSet permanent_folder_types =
      ModelTypeSet(syncer::BOOKMARKS, syncer::NIGORI);

  for (ModelTypeSet::Iterator it = permanent_folder_types.First(); it.Good();
       it.Inc()) {
    ModelType model_type = it.Get();

    std::unique_ptr<LoopbackServerEntity> top_level_entity =
        PersistentPermanentEntity::CreateTopLevel(model_type);
    if (!top_level_entity) {
      return false;
    }
    SaveEntity(std::move(top_level_entity));

    if (model_type == syncer::BOOKMARKS) {
      if (!CreatePermanentBookmarkFolder(kBookmarkBarFolderServerTag,
                                         kBookmarkBarFolderName))
        return false;
      if (!CreatePermanentBookmarkFolder(kOtherBookmarksFolderServerTag,
                                         kOtherBookmarksFolderName))
        return false;
    }
  }

  return true;
}

void LoopbackServer::UpdateEntityVersion(LoopbackServerEntity* entity) {
  entity->SetVersion(++version_);
}

void LoopbackServer::SaveEntity(std::unique_ptr<LoopbackServerEntity> entity) {
  UpdateEntityVersion(entity.get());
  entities_[entity->GetId()] = std::move(entity);
}

void LoopbackServer::HandleCommand(
    const string& request,
    HttpResponse::ServerConnectionCode* server_status,
    int64_t* response_code,
    std::string* response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  sync_pb::ClientToServerMessage message;
  bool parsed = message.ParseFromString(request);
  CHECK(parsed) << "Unable to parse the ClientToServerMessage.";

  sync_pb::ClientToServerResponse response_proto;

  if (message.has_store_birthday() &&
      message.store_birthday() != GetStoreBirthday()) {
    response_proto.set_error_code(sync_pb::SyncEnums::NOT_MY_BIRTHDAY);
  } else {
    bool success = false;
    switch (message.message_contents()) {
      case sync_pb::ClientToServerMessage::GET_UPDATES:
        success = HandleGetUpdatesRequest(message.get_updates(),
                                          response_proto.mutable_get_updates());
        break;
      case sync_pb::ClientToServerMessage::COMMIT:
        success = HandleCommitRequest(message.commit(),
                                      message.invalidator_client_id(),
                                      response_proto.mutable_commit());
        break;
      case sync_pb::ClientToServerMessage::CLEAR_SERVER_DATA:
        ClearServerData();
        response_proto.mutable_clear_server_data();
        success = true;
        break;
      default:
        *server_status = HttpResponse::SYNC_SERVER_ERROR;
        *response_code = net::ERR_NOT_IMPLEMENTED;
        *response = string();
        return;
    }

    if (!success) {
      *server_status = HttpResponse::SYNC_SERVER_ERROR;
      *response_code = net::ERR_FAILED;
      *response = string();
      return;
    }

    response_proto.set_error_code(sync_pb::SyncEnums::SUCCESS);
  }

  response_proto.set_store_birthday(GetStoreBirthday());

  *server_status = HttpResponse::SERVER_CONNECTION_OK;
  *response_code = net::HTTP_OK;
  *response = response_proto.SerializeAsString();

  // TODO(pastarmovj): This should be done asynchronously.
  SaveStateToFile(persistent_file_);
}

bool LoopbackServer::HandleGetUpdatesRequest(
    const sync_pb::GetUpdatesMessage& get_updates,
    sync_pb::GetUpdatesResponse* response) {
  // TODO(pvalenzuela): Implement batching instead of sending all information
  // at once.
  response->set_changes_remaining(0);

  std::unique_ptr<UpdateSieve> sieve = UpdateSieve::Create(get_updates);

  // This folder is called "Synced Bookmarks" by sync and is renamed
  // "Mobile Bookmarks" by the mobile client UIs.
  if (get_updates.create_mobile_bookmarks_folder() &&
      !CreatePermanentBookmarkFolder(kSyncedBookmarksFolderServerTag,
                                     kSyncedBookmarksFolderName)) {
    return false;
  }

  bool send_encryption_keys_based_on_nigori = false;
  int64_t max_response_version = 0;
  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    const LoopbackServerEntity& entity = *it->second;
    if (sieve->ClientWantsItem(entity)) {
      sync_pb::SyncEntity* response_entity = response->add_entries();
      entity.SerializeAsProto(response_entity);

      max_response_version =
          std::max(max_response_version, response_entity->version());

      if (entity.GetModelType() == syncer::NIGORI) {
        send_encryption_keys_based_on_nigori =
            response_entity->specifics().nigori().passphrase_type() ==
            sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE;
      }
    }
  }

  if (send_encryption_keys_based_on_nigori ||
      get_updates.need_encryption_key()) {
    for (vector<string>::iterator it = keystore_keys_.begin();
         it != keystore_keys_.end(); ++it) {
      response->add_encryption_keys(*it);
    }
  }

  sieve->UpdateProgressMarkers(max_response_version, response);
  return true;
}

string LoopbackServer::CommitEntity(
    const sync_pb::SyncEntity& client_entity,
    sync_pb::CommitResponse_EntryResponse* entry_response,
    const string& client_guid,
    const string& parent_id) {
  if (client_entity.version() == 0 && client_entity.deleted()) {
    return string();
  }

  std::unique_ptr<LoopbackServerEntity> entity;
  if (client_entity.deleted()) {
    entity = PersistentTombstoneEntity::Create(client_entity);
    DeleteChildren(client_entity.id_string());
  } else if (GetModelType(client_entity) == syncer::NIGORI) {
    // NIGORI is the only permanent item type that should be updated by the
    // client.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    CHECK(iter != entities_.end());
    entity = PersistentPermanentEntity::CreateUpdatedNigoriEntity(
        client_entity, *iter->second);
  } else if (client_entity.has_client_defined_unique_tag()) {
    entity = PersistentUniqueClientEntity::Create(client_entity);
  } else {
    // TODO(pvalenzuela): Validate entity's parent ID.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    if (iter != entities_.end()) {
      entity = PersistentBookmarkEntity::CreateUpdatedVersion(
          client_entity, *iter->second, parent_id);
    } else {
      entity = PersistentBookmarkEntity::CreateNew(client_entity, parent_id,
                                                   client_guid);
    }
  }

  if (!entity) {
    LOG(ERROR) << "No server entity was created for client entity with type: "
               << GetModelType(client_entity)
               << " and ID: " << client_entity.id_string() << ".";
    return string();
  }

  const std::string id = entity->GetId();
  SaveEntity(std::move(entity));
  BuildEntryResponseForSuccessfulCommit(id, entry_response);
  return id;
}

void LoopbackServer::BuildEntryResponseForSuccessfulCommit(
    const std::string& entity_id,
    sync_pb::CommitResponse_EntryResponse* entry_response) {
  EntityMap::const_iterator iter = entities_.find(entity_id);
  CHECK(iter != entities_.end());
  const LoopbackServerEntity& entity = *iter->second;
  entry_response->set_response_type(sync_pb::CommitResponse::SUCCESS);
  entry_response->set_id_string(entity.GetId());

  if (entity.IsDeleted()) {
    entry_response->set_version(entity.GetVersion() + 1);
  } else {
    entry_response->set_version(entity.GetVersion());
    entry_response->set_name(entity.GetName());
  }
}

bool LoopbackServer::IsChild(const string& id,
                             const string& potential_parent_id) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end()) {
    // We've hit an ID (probably the imaginary root entity) that isn't stored
    // by the server, so it can't be a child.
    return false;
  }

  const LoopbackServerEntity& entity = *iter->second;
  if (entity.GetParentId() == potential_parent_id)
    return true;

  // Recursively look up the tree.
  return IsChild(entity.GetParentId(), potential_parent_id);
}

void LoopbackServer::DeleteChildren(const string& id) {
  std::vector<sync_pb::SyncEntity> tombstones;
  // Find all the children of id.
  for (auto& entity : entities_) {
    if (IsChild(entity.first, id)) {
      sync_pb::SyncEntity proto;
      entity.second->SerializeAsProto(&proto);
      tombstones.emplace_back(proto);
    }
  }

  for (auto& tombstone : tombstones) {
    SaveEntity(PersistentTombstoneEntity::Create(tombstone));
  }
}

bool LoopbackServer::HandleCommitRequest(
    const sync_pb::CommitMessage& commit,
    const std::string& invalidator_client_id,
    sync_pb::CommitResponse* response) {
  std::map<string, string> client_to_server_ids;
  string guid = commit.cache_guid();
  ModelTypeSet committed_model_types;

  // TODO(pvalenzuela): Add validation of CommitMessage.entries.
  ::google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>::const_iterator it;
  for (it = commit.entries().begin(); it != commit.entries().end(); ++it) {
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response->add_entryresponse();

    sync_pb::SyncEntity client_entity = *it;
    string parent_id = client_entity.parent_id_string();
    if (client_to_server_ids.find(parent_id) != client_to_server_ids.end()) {
      parent_id = client_to_server_ids[parent_id];
    }

    const string entity_id =
        CommitEntity(client_entity, entry_response, guid, parent_id);
    if (entity_id.empty()) {
      return false;
    }

    // Record the ID if it was renamed.
    if (entity_id != client_entity.id_string()) {
      client_to_server_ids[client_entity.id_string()] = entity_id;
    }

    EntityMap::const_iterator iter = entities_.find(entity_id);
    CHECK(iter != entities_.end());
    committed_model_types.Put(iter->second->GetModelType());
  }

  return true;
}

void LoopbackServer::ClearServerData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  entities_.clear();
  keystore_keys_.clear();
  ++store_birthday_;
  Init();
}

std::string LoopbackServer::GetStoreBirthday() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Int64ToString(store_birthday_);
}

void LoopbackServer::SerializeState(sync_pb::LoopbackServerProto* proto) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  proto->set_version(kCurrentLoopbackServerProtoVersion);
  proto->set_store_birthday(store_birthday_);
  proto->set_last_version_assigned(version_);
  for (const auto& key : keystore_keys_)
    proto->add_keystore_keys(key);
  for (const auto& entity : entities_) {
    auto* new_entity = proto->mutable_entities()->Add();
    entity.second->SerializeAsLoopbackServerEntity(new_entity);
  }
}

bool LoopbackServer::DeSerializeState(
    const sync_pb::LoopbackServerProto& proto) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(proto.version(), kCurrentLoopbackServerProtoVersion);

  store_birthday_ = proto.store_birthday();
  version_ = proto.last_version_assigned();
  for (int i = 0; i < proto.keystore_keys_size(); ++i)
    keystore_keys_.push_back(proto.keystore_keys(i));
  for (int i = 0; i < proto.entities_size(); ++i) {
    entities_[proto.entities(i).entity().id_string()] =
        LoopbackServerEntity::CreateEntityFromProto(proto.entities(i));
  }

  return true;
}

// Saves all entities and server state to a protobuf file in |filename|.
bool LoopbackServer::SaveStateToFile(const base::FilePath& filename) const {
  sync_pb::LoopbackServerProto proto;
  SerializeState(&proto);

  std::string serialized = proto.SerializeAsString();
  if (!base::CreateDirectory(filename.DirName())) {
    LOG(ERROR) << "Loopback sync could not create the storage directory.";
    return false;
  }
  int result = base::WriteFile(filename, serialized.data(), serialized.size());
  UMA_HISTOGRAM_MEMORY_KB("Sync.Local.FileSize", result);
  return result == static_cast<int>(serialized.size());
}

// Loads all entities and server state from a protobuf file in |filename|.
bool LoopbackServer::LoadStateFromFile(const base::FilePath& filename) {
  if (base::PathExists(filename)) {
    std::string serialized;
    if (base::ReadFileToString(filename, &serialized)) {
      sync_pb::LoopbackServerProto proto;
      if (serialized.length() > 0 && proto.ParseFromString(serialized)) {
        DeSerializeState(proto);
        return true;
      } else {
        LOG(ERROR) << "Loopback sync can not parse the persistent state file.";
        return false;
      }
    } else {
      // TODO(pastarmovj): Try to understand what is the issue e.g. file already
      // open, no access rights etc. and decide if better course of action is
      // available instead of giving up and wiping the global state on the next
      // write.
      LOG(ERROR) << "Loopback sync can not read the persistent state file.";
      return false;
    }
  }
  LOG(WARNING) << "Loopback sync persistent state file does not exist.";
  return false;
}

}  // namespace syncer
