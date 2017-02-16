// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_H_
#define COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/attachments/attachment_store.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

class AttachmentService;
class SyncErrorFactory;

// TODO(zea): remove SupportsWeakPtr in favor of having all SyncableService
// implementers provide a way of getting a weak pointer to themselves.
// See crbug.com/100114.
class SyncableService : public SyncChangeProcessor,
                        public base::SupportsWeakPtr<SyncableService> {
 public:
  // A StartSyncFlare is useful when your SyncableService has a need for sync
  // to start ASAP, typically because a local change event has occurred but
  // MergeDataAndStartSyncing hasn't been called yet, meaning you don't have a
  // SyncChangeProcessor. The sync subsystem will respond soon after invoking
  // Run() on your flare by calling MergeDataAndStartSyncing. The ModelType
  // parameter is included so that the recieving end can track usage and timing
  // statistics, make optimizations or tradeoffs by type, etc.
  using StartSyncFlare = base::Callback<void(ModelType)>;

  // Informs the service to begin syncing the specified synced datatype |type|.
  // The service should then merge |initial_sync_data| into it's local data,
  // calling |sync_processor|'s ProcessSyncChanges as necessary to reconcile the
  // two. After this, the SyncableService's local data should match the server
  // data, and the service should be ready to receive and process any further
  // SyncChange's as they occur.
  // Returns: a SyncMergeResult whose error field reflects whether an error
  //          was encountered while merging the two models. The merge result
  //          may also contain optional merge statistics.
  virtual SyncMergeResult MergeDataAndStartSyncing(
      ModelType type,
      const SyncDataList& initial_sync_data,
      std::unique_ptr<SyncChangeProcessor> sync_processor,
      std::unique_ptr<SyncErrorFactory> error_handler) = 0;

  // Stop syncing the specified type and reset state.
  virtual void StopSyncing(ModelType type) = 0;

  // SyncChangeProcessor interface.
  // Process a list of new SyncChanges and update the local data as necessary.
  // Returns: A default SyncError (IsSet() == false) if no errors were
  //          encountered, and a filled SyncError (IsSet() == true)
  //          otherwise.
  SyncError ProcessSyncChanges(const tracked_objects::Location& from_here,
                               const SyncChangeList& change_list) override = 0;

  // Returns AttachmentStore for use by sync when uploading or downloading
  // attachments.
  // GetAttachmentStoreForSync is called right before MergeDataAndStartSyncing.
  // If at that time GetAttachmentStoreForSync returns null then datatype is
  // considered not using attachments and all attempts to upload/download
  // attachments will fail. Default implementation returns null. Datatype that
  // uses sync attachments should create attachment store, implement
  // GetAttachmentStoreForSync to return result of
  // AttachmentStore::CreateAttachmentStoreForSync() from attachment store
  // object.
  virtual std::unique_ptr<AttachmentStoreForSync> GetAttachmentStoreForSync();

  // Called by sync to provide AttachmentService to be used to download
  // attachments.
  // SetAttachmentService is called after GetAttachmentStore and right before
  // MergeDataAndStartSyncing and only if GetAttachmentStore has returned a
  // non-null store instance. Default implementation does nothing.
  // Datatype that uses attachments must take ownerhip of the provided
  // AttachmentService instance.
  virtual void SetAttachmentService(
      std::unique_ptr<AttachmentService> attachment_service);

 protected:
  ~SyncableService() override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_H_
