// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ENTITY_CHANGE_H_
#define COMPONENTS_SYNC_MODEL_ENTITY_CHANGE_H_

#include <string>
#include <vector>

#include "components/sync/model/entity_data.h"

namespace syncer {

class EntityChange {
 public:
  enum ChangeType { ACTION_ADD, ACTION_UPDATE, ACTION_DELETE };

  static EntityChange CreateAdd(const std::string& storage_key,
                                EntityDataPtr data);
  static EntityChange CreateUpdate(const std::string& storage_key,
                                   EntityDataPtr data);
  static EntityChange CreateDelete(const std::string& storage_key);

  EntityChange(const EntityChange& other);
  virtual ~EntityChange();

  std::string storage_key() const { return storage_key_; }
  ChangeType type() const { return type_; }
  const EntityData& data() const { return data_.value(); }
  // TODO(pavely): data_ptr() is added temporarily to support converting
  // ModelTypeSyncBridge::MergeSyncData from map to change list. Should be
  // removed as part of crbug.com/719570.
  const EntityDataPtr& data_ptr() const { return data_; }

 private:
  EntityChange(const std::string& storage_key,
               ChangeType type,
               EntityDataPtr data);

  std::string storage_key_;
  ChangeType type_;
  EntityDataPtr data_;
};

using EntityChangeList = std::vector<EntityChange>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ENTITY_CHANGE_H_
