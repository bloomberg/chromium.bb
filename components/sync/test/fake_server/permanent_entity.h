// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_PERMANENT_ENTITY_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_PERMANENT_ENTITY_H_

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server_entity.h"

namespace fake_server {

// A server-created, permanent entity.
class PermanentEntity : public FakeServerEntity {
 public:
  ~PermanentEntity() override;

  // Factory function for PermanentEntity. |server_tag| should be a globally
  // unique identifier.
  static std::unique_ptr<FakeServerEntity> Create(
      const syncer::ModelType& model_type,
      const std::string& server_tag,
      const std::string& name,
      const std::string& parent_server_tag);

  // Factory function for a top level PermanentEntity. Top level means that the
  // entity's parent is the root entity (no PermanentEntity exists for root).
  static std::unique_ptr<FakeServerEntity> CreateTopLevel(
      const syncer::ModelType& model_type);

  // Factory function for creating an updated version of a PermanentEntity.
  // This function should only be called for the Nigori entity.
  static std::unique_ptr<FakeServerEntity> CreateUpdatedNigoriEntity(
      const sync_pb::SyncEntity& client_entity,
      const FakeServerEntity& current_server_entity);

  // FakeServerEntity implementation.
  bool RequiresParentId() const override;
  std::string GetParentId() const override;
  void SerializeAsProto(sync_pb::SyncEntity* proto) const override;
  bool IsFolder() const override;
  bool IsPermanent() const override;

 private:
  PermanentEntity(const std::string& id,
                  const syncer::ModelType& model_type,
                  const std::string& name,
                  const std::string& parent_id,
                  const std::string& server_defined_unique_tag,
                  const sync_pb::EntitySpecifics& entity_specifics);

  // All member values have equivalent fields in SyncEntity.
  std::string server_defined_unique_tag_;
  std::string parent_id_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_PERMANENT_ENTITY_H_
