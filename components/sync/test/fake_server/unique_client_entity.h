// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_UNIQUE_CLIENT_ENTITY_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_UNIQUE_CLIENT_ENTITY_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/test/fake_server/fake_server_entity.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}  // namespace sync_pb

namespace fake_server {

// An entity that is unique per client account.
//
// TODO(pvalenzuela): Reconsider the naming of this class. In some cases, this
// type is used to represent entities where the unique client tag is irrelevant
// (e.g., Autofill Wallet).
class UniqueClientEntity : public FakeServerEntity {
 public:
  UniqueClientEntity(const std::string& id,
                     const std::string& client_defined_unique_tag,
                     syncer::ModelType model_type,
                     int64_t version,
                     const std::string& name,
                     const sync_pb::EntitySpecifics& specifics,
                     int64_t creation_time,
                     int64_t last_modified_time);

  ~UniqueClientEntity() override;

  // Factory function for creating a UniqueClientEntity.
  static std::unique_ptr<FakeServerEntity> Create(
      const sync_pb::SyncEntity& client_entity);

  // Factory function for creating a UniqueClientEntity for use in the
  // FakeServer injection API.
  static std::unique_ptr<FakeServerEntity> CreateForInjection(
      const std::string& name,
      const sync_pb::EntitySpecifics& entity_specifics);

  // Derives an ID from a unique client tagged entity.
  static std::string EffectiveIdForClientTaggedEntity(
      const sync_pb::SyncEntity& entity);

  // FakeServerEntity implementation.
  bool RequiresParentId() const override;
  std::string GetParentId() const override;
  void SerializeAsProto(sync_pb::SyncEntity* proto) const override;

 private:
  // These member values have equivalent fields in SyncEntity.
  int64_t creation_time_;
  int64_t last_modified_time_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_UNIQUE_CLIENT_ENTITY_H_
