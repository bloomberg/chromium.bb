// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/data_type_error_handler.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/attachments/attachment_service.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy.h"

namespace syncer {
class SyncData;
class SyncableService;
class WriteNode;
class WriteTransaction;

typedef std::vector<syncer::SyncData> SyncDataList;
}  // namespace syncer

namespace sync_driver {

class SyncApiComponentFactory;
class SyncClient;

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
                               public syncer::AttachmentService::Delegate,
                               public base::NonThreadSafe {
 public:
  // Create a change processor for |type| and connect it to the syncer.
  // |attachment_store| can be NULL which means that datatype will not use sync
  // attachments.
  GenericChangeProcessor(
      syncer::ModelType type,
      DataTypeErrorHandler* error_handler,
      const base::WeakPtr<syncer::SyncableService>& local_service,
      const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
      syncer::UserShare* user_share,
      SyncClient* sync_client,
      std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store);
  ~GenericChangeProcessor() override;

  // ChangeProcessor interface.
  // Build and store a list of all changes into |syncer_changes_|.
  void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64_t version,
      const syncer::ImmutableChangeRecordList& changes) override;
  // Passes |syncer_changes_|, built in ApplyChangesFromSyncModel, onto
  // |local_service_| by way of its ProcessSyncChanges method.
  void CommitChangesFromSyncModel() override;

  // syncer::SyncChangeProcessor implementation.
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError UpdateDataTypeContext(
      syncer::ModelType type,
      syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
      const std::string& context) override;

  // syncer::AttachmentService::Delegate implementation.
  void OnAttachmentUploaded(const syncer::AttachmentId& attachment_id) override;

  // Similar to above, but returns a SyncError for use by direct clients
  // of GenericChangeProcessor that may need more error visibility.
  virtual syncer::SyncError GetAllSyncDataReturnError(
      syncer::SyncDataList* data) const;

  // If a datatype context associated with this GenericChangeProcessor's type
  // exists, fills |context| and returns true. Otheriwse, if there has not been
  // a context set, returns false.
  virtual bool GetDataTypeContext(std::string* context) const;

  // Returns the number of items for this type.
  virtual int GetSyncCount();

  // Generic versions of AssociatorInterface methods. Called by
  // syncer::SyncableServiceAdapter or the DataTypeController.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);
  virtual bool CryptoReadyIfNecessary();

  // Gets AttachmentService proxy for datatype.
  std::unique_ptr<syncer::AttachmentService> GetAttachmentService() const;

 protected:
  // ChangeProcessor interface.
  void StartImpl() override;  // Does nothing.
  syncer::UserShare* share_handle() const override;

 private:
  // Logically part of ProcessSyncChanges.
  //
  // |new_attachments| is an output parameter containing newly added attachments
  // that need to be stored.  This method will append to it.
  syncer::SyncError HandleActionAdd(const syncer::SyncChange& change,
                                    const std::string& type_str,
                                    const syncer::WriteTransaction& trans,
                                    syncer::WriteNode* sync_node,
                                    syncer::AttachmentIdSet* new_attachments);

  // Logically part of ProcessSyncChanges.
  //
  // |new_attachments| is an output parameter containing newly added attachments
  // that need to be stored.  This method will append to it.
  syncer::SyncError HandleActionUpdate(
      const syncer::SyncChange& change,
      const std::string& type_str,
      const syncer::WriteTransaction& trans,
      syncer::WriteNode* sync_node,
      syncer::AttachmentIdSet* new_attachments);

  // Begin uploading attachments that have not yet been uploaded to the sync
  // server.
  void UploadAllAttachmentsNotOnServer();

  const syncer::ModelType type_;

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

  // AttachmentService for datatype. Can be NULL if datatype doesn't use
  // attachments.
  std::unique_ptr<syncer::AttachmentService> attachment_service_;

  // Must be destroyed before attachment_service_ to ensure WeakPtrs are
  // invalidated before attachment_service_ is destroyed.
  // Can be NULL if attachment_service_ is NULL;
  std::unique_ptr<base::WeakPtrFactory<syncer::AttachmentService>>
      attachment_service_weak_ptr_factory_;
  syncer::AttachmentServiceProxy attachment_service_proxy_;
  base::WeakPtrFactory<GenericChangeProcessor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GenericChangeProcessor);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_H_
