// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_BOOKMARK_ENTITY_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_BOOKMARK_ENTITY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server_entity.h"

namespace fake_server {

// A bookmark version of FakeServerEntity. This type represents entities that
// are non-deleted, client-created, and not unique per client account.
class BookmarkEntity : public FakeServerEntity {
 public:
  ~BookmarkEntity() override;

  // Factory function for BookmarkEntity. This factory should be used only for
  // the first time that a specific bookmark is seen by the server.
  static std::unique_ptr<FakeServerEntity> CreateNew(
      const sync_pb::SyncEntity& client_entity,
      const std::string& parent_id,
      const std::string& client_guid);

  // Factory function for BookmarkEntity. The server's current entity for this
  // ID, |current_server_entity|, is passed here because the client does not
  // always send the complete entity over the wire. This requires copying of
  // some of the existing entity when creating a new entity.
  static std::unique_ptr<FakeServerEntity> CreateUpdatedVersion(
      const sync_pb::SyncEntity& client_entity,
      const FakeServerEntity& current_server_entity,
      const std::string& parent_id);

  BookmarkEntity(const std::string& id,
                 int64_t version,
                 const std::string& name,
                 const std::string& originator_cache_guid,
                 const std::string& originator_client_item_id,
                 const sync_pb::UniquePosition& unique_position,
                 const sync_pb::EntitySpecifics& specifics,
                 bool is_folder,
                 const std::string& parent_id,
                 int64_t creation_time,
                 int64_t last_modified_time);

  void SetParentId(const std::string& parent_id);

  // FakeServerEntity implementation.
  bool RequiresParentId() const override;
  std::string GetParentId() const override;
  void SerializeAsProto(sync_pb::SyncEntity* proto) const override;
  bool IsFolder() const override;

 private:
  // All member values have equivalent fields in SyncEntity.
  std::string originator_cache_guid_;
  std::string originator_client_item_id_;
  sync_pb::UniquePosition unique_position_;
  bool is_folder_;
  std::string parent_id_;
  int64_t creation_time_;
  int64_t last_modified_time_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_BOOKMARK_ENTITY_H_
