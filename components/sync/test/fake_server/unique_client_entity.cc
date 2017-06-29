// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/unique_client_entity.h"

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/permanent_entity.h"

using std::string;
using syncer::GenerateSyncableHash;
using syncer::GetModelTypeFromSpecifics;
using syncer::ModelType;

namespace fake_server {

namespace {

// A version must be passed when creating a FakeServerEntity, but this value
// is overrideen immediately when saving the entity in FakeServer.
const int64_t kUnusedVersion = 0L;

// Default time (creation and last modified) used when creating entities.
const int64_t kDefaultTime = 1234L;

}  // namespace

UniqueClientEntity::UniqueClientEntity(
    const string& id,
    const string& client_defined_unique_tag,
    ModelType model_type,
    int64_t version,
    const string& name,
    const sync_pb::EntitySpecifics& specifics,
    int64_t creation_time,
    int64_t last_modified_time)
    : FakeServerEntity(id,
                       client_defined_unique_tag,
                       model_type,
                       version,
                       name),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time) {
  SetSpecifics(specifics);
}

UniqueClientEntity::~UniqueClientEntity() {}

// static
std::unique_ptr<FakeServerEntity> UniqueClientEntity::Create(
    const sync_pb::SyncEntity& client_entity) {
  ModelType model_type =
      syncer::GetModelTypeFromSpecifics(client_entity.specifics());
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
  string id = FakeServerEntity::CreateId(model_type, effective_tag);
  return std::unique_ptr<FakeServerEntity>(new UniqueClientEntity(
      id, effective_tag, model_type, client_entity.version(),
      client_entity.name(), client_entity.specifics(), client_entity.ctime(),
      client_entity.mtime()));
}

// static
std::unique_ptr<FakeServerEntity> UniqueClientEntity::CreateForInjection(
    const string& name,
    const sync_pb::EntitySpecifics& entity_specifics) {
  ModelType model_type = GetModelTypeFromSpecifics(entity_specifics);
  string client_defined_unique_tag = GenerateSyncableHash(model_type, name);
  string id = FakeServerEntity::CreateId(model_type, client_defined_unique_tag);
  return std::unique_ptr<FakeServerEntity>(new UniqueClientEntity(
      id, client_defined_unique_tag, model_type, kUnusedVersion, name,
      entity_specifics, kDefaultTime, kDefaultTime));
}

bool UniqueClientEntity::RequiresParentId() const {
  return false;
}

string UniqueClientEntity::GetParentId() const {
  // The parent ID for this type of entity should always be its ModelType's
  // root node.
  return FakeServerEntity::GetTopLevelId(model_type());
}

void UniqueClientEntity::SerializeAsProto(sync_pb::SyncEntity* proto) const {
  FakeServerEntity::SerializeBaseProtoFields(proto);

  proto->set_ctime(creation_time_);
  proto->set_mtime(last_modified_time_);
}

}  // namespace fake_server
