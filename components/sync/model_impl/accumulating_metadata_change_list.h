// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_ACCUMULATING_METADATA_CHANGE_LIST_H_
#define COMPONENTS_SYNC_MODEL_IMPL_ACCUMULATING_METADATA_CHANGE_LIST_H_

#include <map>
#include <memory>
#include <string>

#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

// A MetadataChangeList implementation that is meant to be used in combination
// with a ModelTypeStore. It accumulates changes in member fields, and then when
// requested transfers them to the store/write batch.
class AccumulatingMetadataChangeList : public InMemoryMetadataChangeList {
 public:
  AccumulatingMetadataChangeList();
  ~AccumulatingMetadataChangeList() override;

  // Moves all currently accumulated changes into the write batch, clear out
  // local copies. Calling this multiple times will work, but should not be
  // necessary.
  void TransferChanges(ModelTypeStore* store,
                       ModelTypeStore::WriteBatch* write_batch);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_ACCUMULATING_METADATA_CHANGE_LIST_H_
