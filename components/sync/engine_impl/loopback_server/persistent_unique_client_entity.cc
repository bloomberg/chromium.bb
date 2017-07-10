// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/hash_util.h"
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
std::unique_ptr<LoopbackServerEntity>
PersistentUniqueClientEntity::CreateFromEntity(
    const sync_pb::SyncEntity& client_entity) {
  ModelType model_type = GetModelTypeFromSpecifics(client_entity.specifics());
  CHECK_NE(client_entity.has_client_defined_unique_tag(),
           syncer::CommitOnlyTypes().Has(model_type))
      << "A UniqueClientEntity should have a client-defined unique tag iff it "
         "is not a CommitOnly type.";
  // Without model type specific logic for each CommitOnly type, we cannot infer
  // a reasonable tag from the specifics. We need uniqueness for how the server
  // holds onto all objects, so simply make a new tag from a random  number.
  string effective_tag = client_entity.has_client_defined_unique_tag()
                             ? client_entity.client_defined_unique_tag()
                             : base::Uint64ToString(base::RandUint64());
  string id = LoopbackServerEntity::CreateId(model_type, effective_tag);
  return std::unique_ptr<LoopbackServerEntity>(new PersistentUniqueClientEntity(
      id, model_type, client_entity.version(), client_entity.name(),
      client_entity.client_defined_unique_tag(), client_entity.specifics(),
      client_entity.ctime(), client_entity.mtime()));
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentUniqueClientEntity::CreateFromEntitySpecifics(
    const string& name,
    const sync_pb::EntitySpecifics& entity_specifics) {
  ModelType model_type = GetModelTypeFromSpecifics(entity_specifics);
  string client_defined_unique_tag = GenerateSyncableHash(model_type, name);
  string id =
      LoopbackServerEntity::CreateId(model_type, client_defined_unique_tag);
  return std::unique_ptr<LoopbackServerEntity>(new PersistentUniqueClientEntity(
      id, model_type, 0, name, client_defined_unique_tag, entity_specifics,
      1337, 1337));
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
