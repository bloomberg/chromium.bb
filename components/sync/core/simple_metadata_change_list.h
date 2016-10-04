// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_SIMPLE_METADATA_CHANGE_LIST_H_
#define COMPONENTS_SYNC_CORE_SIMPLE_METADATA_CHANGE_LIST_H_

#include <map>
#include <memory>
#include <string>

#include "components/sync/api/metadata_change_list.h"
#include "components/sync/api/model_type_store.h"
#include "components/sync/core/non_blocking_sync_common.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

// A MetadataChangeList implementation that is meant to be used in combination
// with a ModelTypeStore. It accumulates changes in member fields, and then when
// requested transfers them to the store/write batch.
class SimpleMetadataChangeList : public MetadataChangeList {
 public:
  enum ChangeType { UPDATE, CLEAR };

  struct MetadataChange {
    ChangeType type;
    sync_pb::EntityMetadata metadata;
  };

  struct ModelTypeStateChange {
    ChangeType type;
    sync_pb::ModelTypeState state;
  };

  typedef std::map<std::string, MetadataChange> MetadataChanges;

  SimpleMetadataChangeList();
  ~SimpleMetadataChangeList() override;

  void UpdateModelTypeState(
      const sync_pb::ModelTypeState& model_type_state) override;
  void ClearModelTypeState() override;
  void UpdateMetadata(const std::string& client_tag,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& client_tag) override;

  const MetadataChanges& GetMetadataChanges() const;
  bool HasModelTypeStateChange() const;
  const ModelTypeStateChange& GetModelTypeStateChange() const;

  // Moves all currently accumulated changes into the write batch, clear out
  // local copies. Calling this multiple times will work, but should not be
  // necessary.
  void TransferChanges(ModelTypeStore* store,
                       ModelTypeStore::WriteBatch* write_batch);

 private:
  MetadataChanges metadata_changes_;
  std::unique_ptr<ModelTypeStateChange> state_change_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_SIMPLE_METADATA_CHANGE_LIST_H_
