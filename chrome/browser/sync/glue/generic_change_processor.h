// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "sync/api/sync_change_processor.h"

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
//
// As a rule, the GenericChangeProcessor is not thread safe, and should only
// be used on the same thread in which it was created.
class GenericChangeProcessor : public ChangeProcessor,
                               public SyncChangeProcessor,
                               public base::NonThreadSafe {
 public:
  // Create a change processor and connect it to the syncer.
  GenericChangeProcessor(DataTypeErrorHandler* error_handler,
                         const base::WeakPtr<SyncableService>& local_service,
                         sync_api::UserShare* user_share);
  virtual ~GenericChangeProcessor();

  // ChangeProcessor interface.
  // Build and store a list of all changes into |syncer_changes_|.
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction* trans,
      const sync_api::ImmutableChangeRecordList& changes) OVERRIDE;
  // Passes |syncer_changes_|, built in ApplyChangesFromSyncModel, onto
  // |local_service_| by way of its ProcessSyncChanges method.
  virtual void CommitChangesFromSyncModel() OVERRIDE;

  // SyncChangeProcessor implementation.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;

  // Fills |current_sync_data| with all the syncer data for the specified type.
  virtual SyncError GetSyncDataForType(syncable::ModelType type,
                                       SyncDataList* current_sync_data);

  // Generic versions of AssociatorInterface methods. Called by
  // SyncableServiceAdapter or the DataTypeController.
  virtual bool SyncModelHasUserCreatedNodes(syncable::ModelType type,
                                            bool* has_nodes);
  virtual bool CryptoReadyIfNecessary(syncable::ModelType type);

 protected:
  // ChangeProcessor interface.
  virtual void StartImpl(Profile* profile) OVERRIDE;           // Does nothing.
  // Called from UI thread (as part of deactivating datatype), but does
  // nothing and is guaranteed to still be alive, so it's okay.
  virtual void StopImpl() OVERRIDE;                            // Does nothing.
  virtual sync_api::UserShare* share_handle() const OVERRIDE;

 private:
  // The SyncableService this change processor will forward changes on to.
  const base::WeakPtr<SyncableService> local_service_;

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
  sync_api::UserShare* const share_handle_;

  DISALLOW_COPY_AND_ASSIGN(GenericChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
