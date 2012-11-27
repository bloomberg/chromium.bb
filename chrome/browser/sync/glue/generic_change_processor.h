// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_merge_result.h"

namespace syncer {
class SyncData;
class SyncableService;

typedef std::vector<syncer::SyncData> SyncDataList;
}  // namespace syncer

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
                               public syncer::SyncChangeProcessor,
                               public base::NonThreadSafe {
 public:
  // Create a change processor and connect it to the syncer.
  GenericChangeProcessor(
      DataTypeErrorHandler* error_handler,
      const base::WeakPtr<syncer::SyncableService>& local_service,
      const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
      syncer::UserShare* user_share);
  virtual ~GenericChangeProcessor();

  // ChangeProcessor interface.
  // Build and store a list of all changes into |syncer_changes_|.
  virtual void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 version,
      const syncer::ImmutableChangeRecordList& changes) OVERRIDE;
  // Passes |syncer_changes_|, built in ApplyChangesFromSyncModel, onto
  // |local_service_| by way of its ProcessSyncChanges method.
  virtual void CommitChangesFromSyncModel() OVERRIDE;

  // syncer::SyncChangeProcessor implementation.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // Fills |current_sync_data| with all the syncer data for the specified type.
  virtual syncer::SyncError GetSyncDataForType(
      syncer::ModelType type,
      syncer::SyncDataList* current_sync_data);

  // Returns the number of items for this type.
  virtual int GetSyncCountForType(syncer::ModelType type);

  // Generic versions of AssociatorInterface methods. Called by
  // syncer::SyncableServiceAdapter or the DataTypeController.
  virtual bool SyncModelHasUserCreatedNodes(syncer::ModelType type,
                                            bool* has_nodes);
  virtual bool CryptoReadyIfNecessary(syncer::ModelType type);

 protected:
  // ChangeProcessor interface.
  virtual void StartImpl(Profile* profile) OVERRIDE;           // Does nothing.
  virtual syncer::UserShare* share_handle() const OVERRIDE;

 private:
  // The SyncableService this change processor will forward changes on to.
  const base::WeakPtr<syncer::SyncableService> local_service_;

  // A SyncMergeResult used to track the changes made during association. The
  // owner will invalidate the weak pointer when association is complete. While
  // the pointer is valid though, we increment it with any changes received
  // via ProcessSyncChanges.
  const base::WeakPtr<syncer::SyncMergeResult> merge_result_;

  // The current list of changes received from the syncer. We buffer because
  // we must ensure no syncapi transaction is held when we pass it on to
  // |local_service_|.
  // Set in ApplyChangesFromSyncModel, consumed in CommitChangesFromSyncModel.
  syncer::SyncChangeList syncer_changes_;

  // Our handle to the sync model. Unlike normal ChangeProcessors, we need to
  // be able to access the sync model before the change processor begins
  // listening to changes (the local_service_ will be interacting with us
  // when it starts up). As such we can't wait until Start(_) has been called,
  // and have to keep a local pointer to the user_share.
  syncer::UserShare* const share_handle_;

  DISALLOW_COPY_AND_ASSIGN(GenericChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
