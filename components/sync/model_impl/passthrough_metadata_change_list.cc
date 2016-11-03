// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/passthrough_metadata_change_list.h"

#include "base/logging.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

PassthroughMetadataChangeList::PassthroughMetadataChangeList(
    ModelTypeStore* store,
    ModelTypeStore::WriteBatch* batch)
    : store_(store), batch_(batch) {
  DCHECK(store_);
  DCHECK(batch_);
}

void PassthroughMetadataChangeList::UpdateModelTypeState(
    const sync_pb::ModelTypeState& model_type_state) {
  store_->WriteGlobalMetadata(batch_, model_type_state.SerializeAsString());
}

void PassthroughMetadataChangeList::ClearModelTypeState() {
  store_->DeleteGlobalMetadata(batch_);
}

void PassthroughMetadataChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  store_->WriteMetadata(batch_, storage_key, metadata.SerializeAsString());
}

void PassthroughMetadataChangeList::ClearMetadata(
    const std::string& storage_key) {
  store_->DeleteMetadata(batch_, storage_key);
}

}  // namespace syncer
