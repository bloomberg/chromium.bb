// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_MODEL_TYPE_PROCESSOR_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/nigori/nigori_local_change_processor.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

class NigoriSyncBridge;

class NigoriModelTypeProcessor : public ModelTypeProcessor,
                                 public ModelTypeControllerDelegate,
                                 public NigoriLocalChangeProcessor {
 public:
  NigoriModelTypeProcessor();
  ~NigoriModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(const sync_pb::ModelTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        UpdateResponseDataList updates) override;

  // ModelTypeControllerDelegate implementation.
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetStatusCountersForDebugging(StatusCountersCallback callback) override;
  void RecordMemoryUsageAndCountsHistograms() override;

  // NigoriLocalChangeProcessor implementation.
  void ModelReadyToSync(
      NigoriSyncBridge* bridge,
      std::pair<sync_pb::ModelTypeState, sync_pb::EntityMetadata>
          nigori_metadata) override;
  void Put(std::unique_ptr<EntityData> entity_data) override;
  std::pair<sync_pb::ModelTypeState, sync_pb::EntityMetadata> GetMetadata()
      override;

 private:
  NigoriSyncBridge* bridge_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NigoriModelTypeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_MODEL_TYPE_PROCESSOR_H_
