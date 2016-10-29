// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/persistent_tombstone_entity.h"

using std::string;

using syncer::ModelType;

namespace syncer {

PersistentTombstoneEntity::~PersistentTombstoneEntity() {}

// static
std::unique_ptr<LoopbackServerEntity> PersistentTombstoneEntity::Create(
    const sync_pb::SyncEntity& entity) {
  const ModelType model_type = GetModelTypeFromId(entity.id_string());
  CHECK_NE(model_type, syncer::UNSPECIFIED) << "Invalid ID was given: "
                                            << entity.id_string();
  return std::unique_ptr<LoopbackServerEntity>(new PersistentTombstoneEntity(
      entity.id_string(), entity.version(), model_type));
}

PersistentTombstoneEntity::PersistentTombstoneEntity(
    const string& id,
    int64_t version,
    const ModelType& model_type)
    : LoopbackServerEntity(id, model_type, version, string()) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(model_type, &specifics);
  SetSpecifics(specifics);
}

bool PersistentTombstoneEntity::RequiresParentId() const {
  return false;
}

string PersistentTombstoneEntity::GetParentId() const {
  return string();
}

void PersistentTombstoneEntity::SerializeAsProto(
    sync_pb::SyncEntity* proto) const {
  LoopbackServerEntity::SerializeBaseProtoFields(proto);
}

bool PersistentTombstoneEntity::IsDeleted() const {
  return true;
}

sync_pb::LoopbackServerEntity_Type
PersistentTombstoneEntity::GetLoopbackServerEntityType() const {
  return sync_pb::LoopbackServerEntity_Type_TOMBSTONE;
}

}  // namespace syncer
