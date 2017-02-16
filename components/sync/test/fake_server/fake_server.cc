// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/fake_server.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "components/sync/test/fake_server/bookmark_entity.h"
#include "components/sync/test/fake_server/permanent_entity.h"
#include "components/sync/test/fake_server/tombstone_entity.h"
#include "components/sync/test/fake_server/unique_client_entity.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

using std::string;
using std::vector;
using syncer::GetModelType;
using syncer::GetModelTypeFromSpecifics;
using syncer::ModelType;
using syncer::ModelTypeSet;

namespace fake_server {

class FakeServerEntity;

namespace {

// The default keystore key.
static const char kDefaultKeystoreKey[] = "1111111111111111";

// Properties of the bookmark bar permanent folder.
static const char kBookmarkBarFolderServerTag[] = "bookmark_bar";
static const char kBookmarkBarFolderName[] = "Bookmark Bar";

// Properties of the other bookmarks permanent folder.
static const char kOtherBookmarksFolderServerTag[] = "other_bookmarks";
static const char kOtherBookmarksFolderName[] = "Other Bookmarks";

// Properties of the synced bookmarks permanent folder.
static const char kSyncedBookmarksFolderServerTag[] = "synced_bookmarks";
static const char kSyncedBookmarksFolderName[] = "Synced Bookmarks";

// A filter used during GetUpdates calls to determine what information to
// send back to the client; filtering out old entities and tracking versions to
// use in response progress markers. Note that only the GetUpdatesMessage's
// from_progress_marker is used to determine this; legacy fields are ignored.
class UpdateSieve {
 public:
  explicit UpdateSieve(const sync_pb::GetUpdatesMessage& message)
      : UpdateSieve(MessageToVersionMap(message)) {}
  ~UpdateSieve() {}

  // Sets the progress markers in |get_updates_response| based on the highest
  // version between request progress markers and response entities.
  void SetProgressMarkers(
      sync_pb::GetUpdatesResponse* get_updates_response) const {
    for (const auto& kv : response_version_map_) {
      sync_pb::DataTypeProgressMarker* new_marker =
          get_updates_response->add_new_progress_marker();
      new_marker->set_data_type_id(
          GetSpecificsFieldNumberFromModelType(kv.first));
      new_marker->set_token(base::Int64ToString(kv.second));
    }
  }

  // Determines whether the server should send an |entity| to the client as
  // part of a GetUpdatesResponse. Update internal tracking of max versions as a
  // side effect which will later be used to set response progress markers.
  bool ClientWantsItem(const FakeServerEntity& entity) {
    int64_t version = entity.GetVersion();
    ModelType type = entity.model_type();
    response_version_map_[type] =
        std::max(response_version_map_[type], version);
    auto it = request_version_map_.find(type);
    return it == request_version_map_.end() ? false : it->second < version;
  }

 private:
  using ModelTypeToVersionMap = std::map<ModelType, int64_t>;

  static UpdateSieve::ModelTypeToVersionMap MessageToVersionMap(
      const sync_pb::GetUpdatesMessage& get_updates_message) {
    CHECK_GT(get_updates_message.from_progress_marker_size(), 0)
        << "A GetUpdates request must have at least one progress marker.";
    ModelTypeToVersionMap request_version_map;

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
      DCHECK(request_version_map.find(model_type) == request_version_map.end());
      request_version_map[model_type] = version;
    }
    return request_version_map;
  }

  explicit UpdateSieve(const ModelTypeToVersionMap request_version_map)
      : request_version_map_(request_version_map),
        response_version_map_(request_version_map) {}

  // The largest versions the client has seen before this request, and is used
  // to filter entities to send back to clients. The values in this map are not
  // updated after being initially set. The presence of a type in this map is a
  // proxy for the desire to receive results about this type.
  const ModelTypeToVersionMap request_version_map_;

  // The largest versions seen between client and server, ultimately used to
  // send progress markers back to the client.
  ModelTypeToVersionMap response_version_map_;
};

// Returns whether |entity| is deleted or permanent.
bool IsDeletedOrPermanent(const FakeServerEntity& entity) {
  return entity.IsDeleted() || entity.IsPermanent();
}

}  // namespace

FakeServer::FakeServer()
    : version_(0),
      store_birthday_(0),
      authenticated_(true),
      error_type_(sync_pb::SyncEnums::SUCCESS),
      alternate_triggered_errors_(false),
      request_counter_(0),
      network_enabled_(true),
      weak_ptr_factory_(this) {
  Init();
}

FakeServer::~FakeServer() {}

void FakeServer::Init() {
  keystore_keys_.push_back(kDefaultKeystoreKey);

  const bool create_result = CreateDefaultPermanentItems();
  DCHECK(create_result) << "Permanent items were not created successfully.";
}

bool FakeServer::CreatePermanentBookmarkFolder(const std::string& server_tag,
                                               const std::string& name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<FakeServerEntity> entity =
      PermanentEntity::Create(syncer::BOOKMARKS, server_tag, name,
                              ModelTypeToRootTag(syncer::BOOKMARKS));
  if (!entity)
    return false;

  SaveEntity(std::move(entity));
  return true;
}

bool FakeServer::CreateDefaultPermanentItems() {
  // Permanent folders are always required for Bookmarks (hierarchical
  // structure) and Nigori (data stored in permanent root folder).
  ModelTypeSet permanent_folder_types =
      ModelTypeSet(syncer::BOOKMARKS, syncer::NIGORI);

  for (ModelTypeSet::Iterator it = permanent_folder_types.First(); it.Good();
       it.Inc()) {
    ModelType model_type = it.Get();

    std::unique_ptr<FakeServerEntity> top_level_entity =
        PermanentEntity::CreateTopLevel(model_type);
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

void FakeServer::UpdateEntityVersion(FakeServerEntity* entity) {
  entity->SetVersion(++version_);
}

void FakeServer::SaveEntity(std::unique_ptr<FakeServerEntity> entity) {
  UpdateEntityVersion(entity.get());
  entities_[entity->id()] = std::move(entity);
}

void FakeServer::HandleCommand(const string& request,
                               const base::Closure& completion_closure,
                               int* error_code,
                               int* response_code,
                               std::string* response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!network_enabled_) {
    *error_code = net::ERR_FAILED;
    *response_code = net::ERR_FAILED;
    *response = string();
    completion_closure.Run();
    return;
  }
  request_counter_++;

  if (!authenticated_) {
    *error_code = 0;
    *response_code = net::HTTP_UNAUTHORIZED;
    *response = string();
    completion_closure.Run();
    return;
  }

  sync_pb::ClientToServerMessage message;
  bool parsed = message.ParseFromString(request);
  CHECK(parsed) << "Unable to parse the ClientToServerMessage.";

  sync_pb::ClientToServerResponse response_proto;

  if (message.has_store_birthday() &&
      message.store_birthday() != GetStoreBirthday()) {
    response_proto.set_error_code(sync_pb::SyncEnums::NOT_MY_BIRTHDAY);
  } else if (error_type_ != sync_pb::SyncEnums::SUCCESS &&
             ShouldSendTriggeredError()) {
    response_proto.set_error_code(error_type_);
  } else if (triggered_actionable_error_.get() && ShouldSendTriggeredError()) {
    sync_pb::ClientToServerResponse_Error* error =
        response_proto.mutable_error();
    error->CopyFrom(*(triggered_actionable_error_.get()));
  } else {
    bool success = false;
    switch (message.message_contents()) {
      case sync_pb::ClientToServerMessage::GET_UPDATES:
        last_getupdates_message_ = message;
        success = HandleGetUpdatesRequest(message.get_updates(),
                                          response_proto.mutable_get_updates());
        break;
      case sync_pb::ClientToServerMessage::COMMIT:
        last_commit_message_ = message;
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
        *error_code = net::ERR_NOT_IMPLEMENTED;
        *response_code = 0;
        *response = string();
        completion_closure.Run();
        return;
    }

    if (!success) {
      // TODO(pvalenzuela): Add logging here so that tests have more info about
      // the failure.
      *error_code = net::ERR_FAILED;
      *response_code = 0;
      *response = string();
      completion_closure.Run();
      return;
    }

    response_proto.set_error_code(sync_pb::SyncEnums::SUCCESS);
  }

  response_proto.set_store_birthday(GetStoreBirthday());

  *error_code = 0;
  *response_code = net::HTTP_OK;
  *response = response_proto.SerializeAsString();
  completion_closure.Run();
}

bool FakeServer::GetLastCommitMessage(sync_pb::ClientToServerMessage* message) {
  if (!last_commit_message_.has_commit())
    return false;

  message->CopyFrom(last_commit_message_);
  return true;
}

bool FakeServer::GetLastGetUpdatesMessage(
    sync_pb::ClientToServerMessage* message) {
  if (!last_getupdates_message_.has_get_updates())
    return false;

  message->CopyFrom(last_getupdates_message_);
  return true;
}

bool FakeServer::HandleGetUpdatesRequest(
    const sync_pb::GetUpdatesMessage& get_updates,
    sync_pb::GetUpdatesResponse* response) {
  // TODO(pvalenzuela): Implement batching instead of sending all information
  // at once.
  response->set_changes_remaining(0);

  auto sieve = base::MakeUnique<UpdateSieve>(get_updates);

  // This folder is called "Synced Bookmarks" by sync and is renamed
  // "Mobile Bookmarks" by the mobile client UIs.
  if (get_updates.create_mobile_bookmarks_folder() &&
      !CreatePermanentBookmarkFolder(kSyncedBookmarksFolderServerTag,
                                     kSyncedBookmarksFolderName)) {
    return false;
  }

  bool send_encryption_keys_based_on_nigori = false;
  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    const FakeServerEntity& entity = *it->second;
    if (sieve->ClientWantsItem(entity)) {
      sync_pb::SyncEntity* response_entity = response->add_entries();
      entity.SerializeAsProto(response_entity);

      if (entity.model_type() == syncer::NIGORI) {
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

  sieve->SetProgressMarkers(response);
  return true;
}

string FakeServer::CommitEntity(
    const sync_pb::SyncEntity& client_entity,
    sync_pb::CommitResponse_EntryResponse* entry_response,
    const string& client_guid,
    const string& parent_id) {
  if (client_entity.version() == 0 && client_entity.deleted()) {
    return string();
  }

  std::unique_ptr<FakeServerEntity> entity;
  if (client_entity.deleted()) {
    entity = TombstoneEntity::Create(client_entity.id_string(),
                                     client_entity.client_defined_unique_tag());
    DeleteChildren(client_entity.id_string());
  } else if (GetModelType(client_entity) == syncer::NIGORI) {
    // NIGORI is the only permanent item type that should be updated by the
    // client.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    CHECK(iter != entities_.end());
    entity = PermanentEntity::CreateUpdatedNigoriEntity(client_entity,
                                                        *iter->second);
  } else if (client_entity.has_client_defined_unique_tag()) {
    entity = UniqueClientEntity::Create(client_entity);
  } else {
    // TODO(pvalenzuela): Validate entity's parent ID.
    EntityMap::const_iterator iter = entities_.find(client_entity.id_string());
    if (iter != entities_.end()) {
      entity = BookmarkEntity::CreateUpdatedVersion(client_entity,
                                                    *iter->second, parent_id);
    } else {
      entity = BookmarkEntity::CreateNew(client_entity, parent_id, client_guid);
    }
  }

  if (!entity) {
    // TODO(pvalenzuela): Add logging so that it is easier to determine why
    // creation failed.
    return string();
  }

  const std::string id = entity->id();
  SaveEntity(std::move(entity));
  BuildEntryResponseForSuccessfulCommit(id, entry_response);
  return id;
}

void FakeServer::BuildEntryResponseForSuccessfulCommit(
    const std::string& entity_id,
    sync_pb::CommitResponse_EntryResponse* entry_response) {
  EntityMap::const_iterator iter = entities_.find(entity_id);
  CHECK(iter != entities_.end());
  const FakeServerEntity& entity = *iter->second;
  entry_response->set_response_type(sync_pb::CommitResponse::SUCCESS);
  entry_response->set_id_string(entity.id());

  if (entity.IsDeleted()) {
    entry_response->set_version(entity.GetVersion() + 1);
  } else {
    entry_response->set_version(entity.GetVersion());
    entry_response->set_name(entity.GetName());
  }
}

bool FakeServer::IsChild(const string& id, const string& potential_parent_id) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end()) {
    // We've hit an ID (probably the imaginary root entity) that isn't stored
    // by the server, so it can't be a child.
    return false;
  }

  const FakeServerEntity& entity = *iter->second;
  if (entity.GetParentId() == potential_parent_id)
    return true;

  // Recursively look up the tree.
  return IsChild(entity.GetParentId(), potential_parent_id);
}

void FakeServer::DeleteChildren(const string& id) {
  std::vector<std::unique_ptr<FakeServerEntity>> tombstones;
  // Find all the children of id.
  for (const auto& entity : entities_) {
    if (IsChild(entity.first, id)) {
      tombstones.push_back(TombstoneEntity::Create(
          entity.first, entity.second->client_defined_unique_tag()));
    }
  }

  for (auto& tombstone : tombstones) {
    SaveEntity(std::move(tombstone));
  }
}

bool FakeServer::HandleCommitRequest(const sync_pb::CommitMessage& commit,
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
    committed_model_types.Put(iter->second->model_type());
  }

  for (auto& observer : observers_)
    observer.OnCommit(invalidator_client_id, committed_model_types);

  return true;
}

std::unique_ptr<base::DictionaryValue>
FakeServer::GetEntitiesAsDictionaryValue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());

  // Initialize an empty ListValue for all ModelTypes.
  ModelTypeSet all_types = ModelTypeSet::All();
  for (ModelTypeSet::Iterator it = all_types.First(); it.Good(); it.Inc()) {
    dictionary->Set(ModelTypeToString(it.Get()), new base::ListValue());
  }

  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    const FakeServerEntity& entity = *it->second;
    if (IsDeletedOrPermanent(entity)) {
      // Tombstones are ignored as they don't represent current data. Folders
      // are also ignored as current verification infrastructure does not
      // consider them.
      continue;
    }
    base::ListValue* list_value;
    if (!dictionary->GetList(ModelTypeToString(entity.model_type()),
                             &list_value)) {
      return std::unique_ptr<base::DictionaryValue>();
    }
    // TODO(pvalenzuela): Store more data for each entity so additional
    // verification can be performed. One example of additional verification
    // is checking the correctness of the bookmark hierarchy.
    list_value->AppendString(entity.GetName());
  }

  return dictionary;
}

std::vector<sync_pb::SyncEntity> FakeServer::GetSyncEntitiesByModelType(
    ModelType model_type) {
  std::vector<sync_pb::SyncEntity> sync_entities;
  DCHECK(thread_checker_.CalledOnValidThread());
  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    const FakeServerEntity& entity = *it->second;
    if (!IsDeletedOrPermanent(entity) && entity.model_type() == model_type) {
      sync_pb::SyncEntity sync_entity;
      entity.SerializeAsProto(&sync_entity);
      sync_entities.push_back(sync_entity);
    }
  }
  return sync_entities;
}

void FakeServer::InjectEntity(std::unique_ptr<FakeServerEntity> entity) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SaveEntity(std::move(entity));
}

bool FakeServer::ModifyEntitySpecifics(
    const std::string& id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end() ||
      iter->second->model_type() !=
          GetModelTypeFromSpecifics(updated_specifics)) {
    return false;
  }

  FakeServerEntity* entity = iter->second.get();
  entity->SetSpecifics(updated_specifics);
  UpdateEntityVersion(entity);
  return true;
}

bool FakeServer::ModifyBookmarkEntity(
    const std::string& id,
    const std::string& parent_id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  EntityMap::const_iterator iter = entities_.find(id);
  if (iter == entities_.end() ||
      iter->second->model_type() != syncer::BOOKMARKS ||
      GetModelTypeFromSpecifics(updated_specifics) != syncer::BOOKMARKS) {
    return false;
  }

  BookmarkEntity* entity = static_cast<BookmarkEntity*>(iter->second.get());

  entity->SetParentId(parent_id);
  entity->SetSpecifics(updated_specifics);
  if (updated_specifics.has_bookmark()) {
    entity->SetName(updated_specifics.bookmark().title());
  }
  UpdateEntityVersion(entity);
  return true;
}

void FakeServer::ClearServerData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  entities_.clear();
  keystore_keys_.clear();
  ++store_birthday_;
  Init();
}

void FakeServer::SetAuthenticated() {
  DCHECK(thread_checker_.CalledOnValidThread());
  authenticated_ = true;
}

void FakeServer::SetUnauthenticated() {
  DCHECK(thread_checker_.CalledOnValidThread());
  authenticated_ = false;
}

bool FakeServer::TriggerError(const sync_pb::SyncEnums::ErrorType& error_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (triggered_actionable_error_.get()) {
    DVLOG(1) << "Only one type of error can be triggered at any given time.";
    return false;
  }

  error_type_ = error_type;
  return true;
}

bool FakeServer::TriggerActionableError(
    const sync_pb::SyncEnums::ErrorType& error_type,
    const string& description,
    const string& url,
    const sync_pb::SyncEnums::Action& action) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (error_type_ != sync_pb::SyncEnums::SUCCESS) {
    DVLOG(1) << "Only one type of error can be triggered at any given time.";
    return false;
  }

  sync_pb::ClientToServerResponse_Error* error =
      new sync_pb::ClientToServerResponse_Error();
  error->set_error_type(error_type);
  error->set_error_description(description);
  error->set_url(url);
  error->set_action(action);
  triggered_actionable_error_.reset(error);
  return true;
}

bool FakeServer::EnableAlternatingTriggeredErrors() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (error_type_ == sync_pb::SyncEnums::SUCCESS &&
      !triggered_actionable_error_.get()) {
    DVLOG(1) << "No triggered error set. Alternating can't be enabled.";
    return false;
  }

  alternate_triggered_errors_ = true;
  // Reset the counter so that the the first request yields a triggered error.
  request_counter_ = 0;
  return true;
}

bool FakeServer::ShouldSendTriggeredError() const {
  if (!alternate_triggered_errors_)
    return true;

  // Check that the counter is odd so that we trigger an error on the first
  // request after alternating is enabled.
  return request_counter_ % 2 != 0;
}

void FakeServer::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void FakeServer::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void FakeServer::EnableNetwork() {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_enabled_ = true;
}

void FakeServer::DisableNetwork() {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_enabled_ = false;
}

std::string FakeServer::GetBookmarkBarFolderId() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (EntityMap::const_iterator it = entities_.begin(); it != entities_.end();
       ++it) {
    FakeServerEntity* entity = it->second.get();
    if (entity->GetName() == kBookmarkBarFolderName && entity->IsFolder() &&
        entity->model_type() == syncer::BOOKMARKS) {
      return entity->id();
    }
  }
  NOTREACHED() << "Bookmark Bar entity not found.";
  return "";
}

base::WeakPtr<FakeServer> FakeServer::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

std::string FakeServer::GetStoreBirthday() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Int64ToString(store_birthday_);
}

}  // namespace fake_server
