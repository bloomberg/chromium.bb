// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_PASSTHROUGH_METADATA_CHANGE_LIST_H_
#define COMPONENTS_SYNC_MODEL_IMPL_PASSTHROUGH_METADATA_CHANGE_LIST_H_

#include <string>

#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {

// A thin wrapper around ModelTypeStore and WriteBatch to implement the
// MetadataChangeList interface. This class should never outlive any of the
// delegate objects it calls.
class PassthroughMetadataChangeList : public MetadataChangeList {
 public:
  PassthroughMetadataChangeList(ModelTypeStore* store,
                                ModelTypeStore::WriteBatch* batch);

  // DataBatch MetadataChangeList.
  void UpdateModelTypeState(
      const sync_pb::ModelTypeState& model_type_state) override;
  void ClearModelTypeState() override;
  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& storage_key) override;

 private:
  // Non owning references.
  ModelTypeStore* store_;
  ModelTypeStore::WriteBatch* batch_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_PASSTHROUGH_METADATA_CHANGE_LIST_H_
