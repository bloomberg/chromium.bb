// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/core/simple_metadata_change_list.h"

namespace syncer {

SimpleMetadataChangeList::SimpleMetadataChangeList() {}

SimpleMetadataChangeList::~SimpleMetadataChangeList() {}

void SimpleMetadataChangeList::UpdateModelTypeState(
    const sync_pb::ModelTypeState& model_type_state) {
  state_change_.reset(new ModelTypeStateChange{UPDATE, model_type_state});
}

void SimpleMetadataChangeList::ClearModelTypeState() {
  state_change_.reset(new ModelTypeStateChange{CLEAR});
}

void SimpleMetadataChangeList::UpdateMetadata(
    const std::string& client_tag,
    const sync_pb::EntityMetadata& metadata) {
  metadata_changes_[client_tag] = {UPDATE, metadata};
}

void SimpleMetadataChangeList::ClearMetadata(const std::string& client_tag) {
  metadata_changes_[client_tag] = {CLEAR, sync_pb::EntityMetadata()};
}

const SimpleMetadataChangeList::MetadataChanges&
SimpleMetadataChangeList::GetMetadataChanges() const {
  return metadata_changes_;
}

bool SimpleMetadataChangeList::HasModelTypeStateChange() const {
  return state_change_.get() != nullptr;
}

const SimpleMetadataChangeList::ModelTypeStateChange&
SimpleMetadataChangeList::GetModelTypeStateChange() const {
  return *state_change_.get();
}

void SimpleMetadataChangeList::TransferChanges(
    ModelTypeStore* store,
    ModelTypeStore::WriteBatch* write_batch) {
  DCHECK(write_batch);
  DCHECK(store);
  for (const auto& pair : metadata_changes_) {
    const std::string& key = pair.first;
    const MetadataChange& change = pair.second;
    switch (change.type) {
      case UPDATE:
        store->WriteMetadata(write_batch, key,
                             change.metadata.SerializeAsString());
        break;
      case CLEAR:
        store->DeleteMetadata(write_batch, key);
        break;
    }
  }
  metadata_changes_.clear();
  if (HasModelTypeStateChange()) {
    switch (state_change_->type) {
      case UPDATE:
        store->WriteGlobalMetadata(write_batch,
                                   state_change_->state.SerializeAsString());
        break;
      case CLEAR:
        store->DeleteGlobalMetadata(write_batch);
        break;
    }
    state_change_.reset();
  }
}

}  // namespace syncer
