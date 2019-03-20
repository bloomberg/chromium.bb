// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_LOCAL_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_LOCAL_CHANGE_PROCESSOR_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

class NigoriSyncBridge;
struct EntityData;

// Interface analogous to ModelTypeChangeProcessor for Nigori, used to propagate
// local changes from the bridge to the processor.
class NigoriLocalChangeProcessor {
 public:
  NigoriLocalChangeProcessor() = default;

  virtual ~NigoriLocalChangeProcessor() = default;

  // The Nigori model is expected to call this method as soon as possible during
  // initialization, and must be called before invoking Put().
  virtual void ModelReadyToSync(
      NigoriSyncBridge* bridge,
      std::pair<sync_pb::ModelTypeState, sync_pb::EntityMetadata>
          nigori_metadata) = 0;

  // Informs the Nigori processor of a new or updated Nigori entity.
  virtual void Put(std::unique_ptr<EntityData> entity_data) = 0;

  // Returns both the entity metadata and model type state such that the Nigori
  // model takes care of persisting them.
  virtual std::pair<sync_pb::ModelTypeState, sync_pb::EntityMetadata>
  GetMetadata() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NigoriLocalChangeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_LOCAL_CHANGE_PROCESSOR_H_
