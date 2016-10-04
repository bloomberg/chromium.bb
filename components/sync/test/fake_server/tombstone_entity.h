// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_TOMBSTONE_ENTITY_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_TOMBSTONE_ENTITY_H_

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server_entity.h"

namespace fake_server {

// A Sync entity that represents a deleted item.
class TombstoneEntity : public FakeServerEntity {
 public:
  ~TombstoneEntity() override;

  // Factory function for TombstoneEntity.
  static std::unique_ptr<FakeServerEntity> Create(
      const std::string& id,
      const std::string& client_defined_unique_tag);

  // FakeServerEntity implementation.
  bool RequiresParentId() const override;
  std::string GetParentId() const override;
  void SerializeAsProto(sync_pb::SyncEntity* proto) const override;
  bool IsDeleted() const override;

 private:
  TombstoneEntity(const std::string& id,
                  const std::string& client_defined_unique_tag,
                  const syncer::ModelType& model_type);
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_TOMBSTONE_ENTITY_H_
