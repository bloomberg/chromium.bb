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
#include "base/sequence_checker.h"
#include "components/sync/model/attachments/attachment_service.h"
#include "components/sync/model/attachments/attachment_service_proxy.h"
#include "components/sync/model/attachments/attachment_store.h"
#include "components/sync/model/change_processor.h"
#include "components/sync/model/data_type_error_handler.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

class SyncClient;
class SyncData;
class SyncableService;
class WriteNode;
class WriteTransaction;

namespace syncable {
class Entry;
}  // namespace syncable

using SyncDataList = std::vector<SyncData>;

// Datatype agnostic change processor. One instance of GenericChangeProcessor
// is created for each datatype and lives on the datatype's sequence. It then
// handles all interaction with the sync api, both translating pushes from the
// local service into transactions and receiving changes from the sync model,
// which then get converted into SyncChange's and sent to the local service.
//
// As a rule, the GenericChangeProcessor is not thread safe, and should only
// be used on the same sequence in which it was created.
class GenericChangeProcessor : public ChangeProcessor,
                               public SyncChangeProcessor,
                               public AttachmentService::Delegate {
 public:
  // Create a change processor for |type| and connect it to the syncer.
  // |attachment_store| can be null which means that datatype will not use sync
  // attachments.
  GenericChangeProcessor(
      ModelType type,
      std::unique_ptr<DataTypeErrorHandler> error_handler,
      const base::WeakPtr<SyncableService>& local_service,
      const base::WeakPtr<SyncMergeResult>& merge_result,
      UserShare* user_share,
      SyncClient* sync_client,
      std::unique_ptr<AttachmentStoreForSync> attachment_store);
  ~GenericChangeProcessor() override;

  // ChangeProcessor interface.
  // Build and store a list of all changes into |syncer_changes_|.
  void ApplyChangesFromSyncModel(
      const BaseTransaction* trans,
      int64_t version,
      const ImmutableChangeRecordList& changes) override;
  // Passes |syncer_changes_|, built in ApplyChangesFromSyncModel, onto
  // |local_service_| by way of its ProcessSyncChanges method.
  void CommitChangesFromSyncModel() override;

  // SyncChangeProcessor implementation.
  SyncError ProcessSyncChanges(const tracked_objects::Location& from_here,
                               const SyncChangeList& change_list) override;
  SyncDataList GetAllSyncData(ModelType type) const override;
  SyncError UpdateDataTypeContext(
      ModelType type,
      SyncChangeProcessor::ContextRefreshStatus refresh_status,
      const std::string& context) override;
  void AddLocalChangeObserver(LocalChangeObserver* observer) override;
  void RemoveLocalChangeObserver(LocalChangeObserver* observer) override;

  // AttachmentService::Delegate implementation.
  void OnAttachmentUploaded(const AttachmentId& attachment_id) override;

  // Similar to above, but returns a SyncError for use by direct clients
  // of GenericChangeProcessor that may need more error visibility.
  virtual SyncError GetAllSyncDataReturnError(SyncDataList* data) const;

  // If a datatype context associated with this GenericChangeProcessor's type
  // exists, fills |context| and returns true. Otheriwse, if there has not been
  // a context set, returns false.
  virtual bool GetDataTypeContext(std::string* context) const;

  // Returns the number of items for this type.
  virtual int GetSyncCount();

  // Generic versions of AssociatorInterface methods. Called by
  // SyncableServiceAdapter or the DataTypeController.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);
  virtual bool CryptoReadyIfNecessary();

  // Gets AttachmentService proxy for datatype.
  std::unique_ptr<AttachmentService> GetAttachmentService() const;

 protected:
  // ChangeProcessor interface.
  void StartImpl() override;  // Does nothing.
  UserShare* share_handle() const override;

 private:
  SyncError AttemptDelete(const SyncChange& change,
                          ModelType type,
                          const std::string& type_str,
                          WriteNode* node,
                          DataTypeErrorHandler* error_handler);
  // Logically part of ProcessSyncChanges.
  //
  // |new_attachments| is an output parameter containing newly added attachments
  // that need to be stored.  This method will append to it.
  SyncError HandleActionAdd(const SyncChange& change,
                            const std::string& type_str,
                            const WriteTransaction& trans,
                            WriteNode* sync_node,
                            AttachmentIdSet* new_attachments);

  // Logically part of ProcessSyncChanges.
  //
  // |new_attachments| is an output parameter containing newly added attachments
  // that need to be stored.  This method will append to it.
  SyncError HandleActionUpdate(const SyncChange& change,
                               const std::string& type_str,
                               const WriteTransaction& trans,
                               WriteNode* sync_node,
                               AttachmentIdSet* new_attachments);

  // Begin uploading attachments that have not yet been uploaded to the sync
  // server.
  void UploadAllAttachmentsNotOnServer();

  // Notify every registered local change observer that |change| is about to be
  // applied to |current_entry|.
  void NotifyLocalChangeObservers(const syncable::Entry* current_entry,
                                  const SyncChange& change);

  base::SequenceChecker sequence_checker_;

  const ModelType type_;

  // The SyncableService this change processor will forward changes on to.
  const base::WeakPtr<SyncableService> local_service_;

  // A SyncMergeResult used to track the changes made during association. The
  // owner will invalidate the weak pointer when association is complete. While
  // the pointer is valid though, we increment it with any changes received
  // via ProcessSyncChanges.
  const base::WeakPtr<SyncMergeResult> merge_result_;

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
  UserShare* const share_handle_;

  // AttachmentService for datatype. Can be null if datatype doesn't use
  // attachments.
  std::unique_ptr<AttachmentService> attachment_service_;

  // List of observers that want to be notified of local changes being written.
  base::ObserverList<LocalChangeObserver> local_change_observers_;

  // Must be destroyed before attachment_service_ to ensure WeakPtrs are
  // invalidated before attachment_service_ is destroyed.
  // Can be null if attachment_service_ is null;
  std::unique_ptr<base::WeakPtrFactory<AttachmentService>>
      attachment_service_weak_ptr_factory_;
  AttachmentServiceProxy attachment_service_proxy_;
  base::WeakPtrFactory<GenericChangeProcessor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GenericChangeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_H_
