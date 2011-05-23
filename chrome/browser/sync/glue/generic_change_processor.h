// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/api/sync_change_processor.h"
#include "chrome/browser/sync/glue/change_processor.h"

class SyncData;
class SyncableService;

typedef std::vector<SyncData> SyncDataList;

namespace browser_sync {

// TODO(sync): deprecate all change processors and have them replaced by
// instances of this.
// Datatype agnostic change processor. One instance of GenericChangeProcessor
// is created for each datatype and lives on the datatype's thread. It then
// handles all interaction with the sync api, both translating pushes from the
// local service into transactions and receiving changes from the sync model,
// which then get converted into SyncChange's and sent to the local service.
class GenericChangeProcessor : public ChangeProcessor,
                               public SyncChangeProcessor {
 public:
  GenericChangeProcessor(SyncableService* local_service,
                         UnrecoverableErrorHandler* error_handler,
                         sync_api::UserShare* user_share);
  ~GenericChangeProcessor();

  // ChangeProcessor interface.
  // Build and store a list of all changes into |syncer_changes_|.
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count) OVERRIDE;
  // Passes |syncer_changes_|, built in ApplyChangesFromSyncModel, onto
  // |local_service_| by way of it's ProcessSyncChanges method.
  virtual void CommitChangesFromSyncModel() OVERRIDE;

  // SyncChangeProcessor implementation.
  virtual void ProcessSyncChanges(const SyncChangeList& change_list) OVERRIDE;

  // Fills |current_sync_data| with all the syncer data for the specified type.
  virtual bool GetSyncDataForType(syncable::ModelType type,
                                  SyncDataList* current_sync_data);

  // Generic versions of AssociatorInterface methods. Called by
  // SyncableServiceAdapter.
  bool SyncModelHasUserCreatedNodes(syncable::ModelType type,
                                    bool* has_nodes);
  bool CryptoReadyIfNecessary(syncable::ModelType type);
 protected:
  // ChangeProcessor interface.
  virtual void StartImpl(Profile* profile) OVERRIDE;  // Not implemented.
  virtual void StopImpl() OVERRIDE;  // Not implemented.
  virtual sync_api::UserShare* share_handle() OVERRIDE;
 private:
  // The SyncableService this change processor will forward changes on to.
  SyncableService* local_service_;

  // The current list of changes received from the syncer. We buffer because
  // we must ensure no syncapi transaction is held when we pass it on to
  // |local_service_|.
  // Set in ApplyChangesFromSyncModel, consumed in CommitChangesFromSyncModel.
  SyncChangeList syncer_changes_;

  // Our handle to the sync model. Unlike normal ChangeProcessors, we need to
  // be able to access the sync model before the change processor begins
  // listening to changes (the local_service_ will be interacting with us
  // when it starts up). As such we can't wait until Start(_) has been called,
  // and have to keep a local pointer to the user_share.
  sync_api::UserShare* user_share_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
