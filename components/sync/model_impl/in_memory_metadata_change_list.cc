// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/in_memory_metadata_change_list.h"

namespace syncer {

InMemoryMetadataChangeList::InMemoryMetadataChangeList() {}
InMemoryMetadataChangeList::~InMemoryMetadataChangeList() {}

void InMemoryMetadataChangeList::TransferChangesTo(MetadataChangeList* other) {
  DCHECK(other);
  for (const auto& pair : metadata_changes_) {
    const std::string& storage_key = pair.first;
    const MetadataChange& change = pair.second;
    switch (change.type) {
      case UPDATE:
        other->UpdateMetadata(storage_key, change.metadata);
        break;
      case CLEAR:
        other->ClearMetadata(storage_key);
        break;
    }
  }
  metadata_changes_.clear();
  if (state_change_) {
    switch (state_change_->type) {
      case UPDATE:
        other->UpdateModelTypeState(state_change_->state);
        break;
      case CLEAR:
        other->ClearModelTypeState();
        break;
    }
    state_change_.reset();
  }
}

void InMemoryMetadataChangeList::UpdateModelTypeState(
    const sync_pb::ModelTypeState& model_type_state) {
  state_change_.reset(new ModelTypeStateChange{UPDATE, model_type_state});
}

void InMemoryMetadataChangeList::ClearModelTypeState() {
  state_change_.reset(new ModelTypeStateChange{CLEAR});
}

void InMemoryMetadataChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  metadata_changes_[storage_key] = {UPDATE, metadata};
}

void InMemoryMetadataChangeList::ClearMetadata(const std::string& storage_key) {
  metadata_changes_[storage_key] = {CLEAR, sync_pb::EntityMetadata()};
}

}  // namespace syncer
