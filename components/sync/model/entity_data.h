// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ENTITY_DATA_H_
#define COMPONENTS_SYNC_MODEL_ENTITY_DATA_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/proto_value_ptr.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

struct EntityData;

struct EntityDataTraits {
  static void SwapValue(EntityData* dest, EntityData* src);
  static bool HasValue(const EntityData& value);
  static const EntityData& DefaultValue();
};

using EntityDataPtr = ProtoValuePtr<EntityData, EntityDataTraits>;
using EntityDataList = std::vector<EntityDataPtr>;

// A light-weight container for sync entity data which represents either
// local data created on the ModelTypeSyncBridge side or remote data created
// on ModelTypeWorker.
// EntityData is supposed to be wrapped and passed by reference.
struct EntityData {
 public:
  EntityData();
  ~EntityData();

  // Typically this is a server assigned sync ID, although for a local change
  // that represents a new entity this field might be either empty or contain
  // a temporary client sync ID.
  std::string id;

  // A hash based on the client tag and model type.
  // Used for various map lookups. Should always be available.
  // Sent to the server as SyncEntity::client_defined_unique_tag.
  std::string client_tag_hash;

  // Entity name, used mostly for Debug purposes.
  std::string non_unique_name;

  // Model type specific sync data.
  sync_pb::EntitySpecifics specifics;

  // Entity creation and modification timestamps.
  base::Time creation_time;
  base::Time modification_time;

  // Sync ID of the parent entity. This is supposed to be set only for
  // hierarchical datatypes (e.g. Bookmarks).
  std::string parent_id;

  // Indicate whether this is a folder or not. Relevant only for bookmarks.
  bool is_folder = false;

  // Unique position of an entity among its siblings. This is supposed to be
  // set only for datatypes that support positioning (e.g. Bookmarks).
  sync_pb::UniquePosition unique_position;

  // True if EntityData represents deleted entity; otherwise false.
  // Note that EntityData would be considered to represent a deletion if its
  // specifics hasn't been set.
  bool is_deleted() const { return specifics.ByteSize() == 0; }

  // Transfers this struct's data to EntityDataPtr.
  // The return value must be assigned into another EntityDataPtr.
  EntityDataPtr PassToPtr() WARN_UNUSED_RESULT;

  // Makes a copy of EntityData and updates its id to |new_id|. This is needed
  // when entity id is updated with commit response while EntityData for next
  // local change is cached in ProcessorEntityTracker.
  EntityDataPtr UpdateId(const std::string& new_id) const WARN_UNUSED_RESULT;

  // Makes a copy of EntityData and updates its specifics to |new_specifics|.
  // This is needed when specifics is updated after decryption in the
  // ModelTypeWorker::DecryptStoredEntities().
  EntityDataPtr UpdateSpecifics(
      const sync_pb::EntitySpecifics& new_specifics) const WARN_UNUSED_RESULT;

  // Dumps all info into a DictionaryValue and returns it.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue();

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  friend struct EntityDataTraits;
  // Used to transfer the data without copying.
  void Swap(EntityData* other);

  // Allow copy ctor so that UpdateId and UpdateSpecifics can make a copy of
  // this EntityData.
  EntityData(const EntityData& src);

  DISALLOW_ASSIGN(EntityData);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ENTITY_DATA_H_
