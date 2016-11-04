// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/accumulating_metadata_change_list.h"

namespace syncer {

AccumulatingMetadataChangeList::AccumulatingMetadataChangeList() {}
AccumulatingMetadataChangeList::~AccumulatingMetadataChangeList() {}

void AccumulatingMetadataChangeList::TransferChanges(
    ModelTypeStore* store,
    ModelTypeStore::WriteBatch* write_batch) {
  DCHECK(write_batch);
  DCHECK(store);
  for (const auto& pair : metadata_changes_) {
    const std::string& storage_key = pair.first;
    const MetadataChange& change = pair.second;
    switch (change.type) {
      case UPDATE:
        store->WriteMetadata(write_batch, storage_key,
                             change.metadata.SerializeAsString());
        break;
      case CLEAR:
        store->DeleteMetadata(write_batch, storage_key);
        break;
    }
  }
  metadata_changes_.clear();
  if (state_change_) {
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
