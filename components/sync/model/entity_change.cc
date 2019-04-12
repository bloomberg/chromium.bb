// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/entity_change.h"

#include "base/memory/ptr_util.h"

namespace syncer {

// static
std::unique_ptr<EntityChange> EntityChange::CreateAdd(
    const std::string& storage_key,
    EntityDataPtr data) {
  return base::WrapUnique(new EntityChange(storage_key, ACTION_ADD, data));
}

// static
std::unique_ptr<EntityChange> EntityChange::CreateUpdate(
    const std::string& storage_key,
    EntityDataPtr data) {
  return base::WrapUnique(new EntityChange(storage_key, ACTION_UPDATE, data));
}

// static
std::unique_ptr<EntityChange> EntityChange::CreateDelete(
    const std::string& storage_key) {
  return base::WrapUnique(
      new EntityChange(storage_key, ACTION_DELETE, EntityDataPtr()));
}

EntityChange::EntityChange(const std::string& storage_key,
                           ChangeType type,
                           EntityDataPtr data)
    : storage_key_(storage_key), type_(type), data_(data) {}

EntityChange::~EntityChange() {}

}  // namespace syncer
