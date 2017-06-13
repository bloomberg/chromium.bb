// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"

#include "base/guid.h"
#include "components/sync/engine_impl/loopback_server/persistent_permanent_entity.h"
#include "components/sync/protocol/sync.pb.h"

using std::string;

namespace syncer {

PersistentUniqueClientEntity::PersistentUniqueClientEntity(
    const string& id,
    ModelType model_type,
    int64_t version,
    const string& name,
    const string& client_defined_unique_tag,
    const sync_pb::EntitySpecifics& specifics,
    int64_t creation_time,
    int64_t last_modified_time)
    : LoopbackServerEntity(id, model_type, version, name),
      client_defined_unique_tag_(client_defined_unique_tag),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time) {
  SetSpecifics(specifics);
}

PersistentUniqueClientEntity::~PersistentUniqueClientEntity() {}

// static
std::unique_ptr<LoopbackServerEntity> PersistentUniqueClientEntity::Create(
    const sync_pb::SyncEntity& client_entity) {
  CHECK(client_entity.has_client_defined_unique_tag())
      << "A PersistentUniqueClientEntity must have a client-defined unique "
         "tag.";
  ModelType model_type = GetModelTypeFromSpecifics(client_entity.specifics());
  string id = EffectiveIdForClientTaggedEntity(client_entity);
  return std::unique_ptr<LoopbackServerEntity>(new PersistentUniqueClientEntity(
      id, model_type, client_entity.version(), client_entity.name(),
      client_entity.client_defined_unique_tag(), client_entity.specifics(),
      client_entity.ctime(), client_entity.mtime()));
}

// static
std::string PersistentUniqueClientEntity::EffectiveIdForClientTaggedEntity(
    const sync_pb::SyncEntity& entity) {
  return LoopbackServerEntity::CreateId(
      GetModelTypeFromSpecifics(entity.specifics()),
      entity.client_defined_unique_tag());
}

bool PersistentUniqueClientEntity::RequiresParentId() const {
  return false;
}

string PersistentUniqueClientEntity::GetParentId() const {
  // The parent ID for this type of entity should always be its ModelType's
  // root node.
  return LoopbackServerEntity::GetTopLevelId(GetModelType());
}

sync_pb::LoopbackServerEntity_Type
PersistentUniqueClientEntity::GetLoopbackServerEntityType() const {
  return sync_pb::LoopbackServerEntity_Type_UNIQUE;
}

void PersistentUniqueClientEntity::SerializeAsProto(
    sync_pb::SyncEntity* proto) const {
  LoopbackServerEntity::SerializeBaseProtoFields(proto);

  proto->set_client_defined_unique_tag(client_defined_unique_tag_);
  proto->set_ctime(creation_time_);
  proto->set_mtime(last_modified_time_);
}

}  // namespace syncer
