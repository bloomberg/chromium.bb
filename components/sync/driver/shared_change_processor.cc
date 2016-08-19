// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/shared_change_processor.h"

#include <memory>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/api/sync_change.h"
#include "components/sync/api/syncable_service.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/core/data_type_error_handler.h"
#include "components/sync/driver/generic_change_processor.h"
#include "components/sync/driver/generic_change_processor_factory.h"
#include "components/sync/driver/shared_change_processor_ref.h"
#include "components/sync/driver/sync_client.h"

using base::AutoLock;

namespace syncer {
class AttachmentService;
}

namespace sync_driver {

SharedChangeProcessor::SharedChangeProcessor(syncer::ModelType type)
    : disconnected_(false),
      type_(type),
      frontend_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      generic_change_processor_(NULL),
      error_handler_(NULL) {
  DCHECK_NE(type_, syncer::UNSPECIFIED);
}

SharedChangeProcessor::~SharedChangeProcessor() {
  // We can either be deleted when the DTC is destroyed (on UI
  // thread), or when the syncer::SyncableService stops syncing (datatype
  // thread).  |generic_change_processor_|, if non-NULL, must be
  // deleted on |backend_loop_|.
  if (backend_task_runner_.get()) {
    if (backend_task_runner_->BelongsToCurrentThread()) {
      delete generic_change_processor_;
    } else {
      DCHECK(frontend_task_runner_->BelongsToCurrentThread());
      if (!backend_task_runner_->DeleteSoon(FROM_HERE,
                                            generic_change_processor_)) {
        NOTREACHED();
      }
    }
  } else {
    DCHECK(!generic_change_processor_);
  }
}

void SharedChangeProcessor::StartAssociation(
    StartDoneCallback start_done,
    SyncClient* const sync_client,
    syncer::UserShare* user_share,
    syncer::DataTypeErrorHandler* error_handler) {
  DCHECK(user_share);
  syncer::SyncMergeResult local_merge_result(type_);
  syncer::SyncMergeResult syncer_merge_result(type_);
  base::WeakPtrFactory<syncer::SyncMergeResult> weak_ptr_factory(
      &syncer_merge_result);

  // Connect |shared_change_processor| to the syncer and get the
  // syncer::SyncableService associated with type_.
  // Note that it's possible the shared_change_processor has already been
  // disconnected at this point, so all our accesses to the syncer from this
  // point on are through it.
  GenericChangeProcessorFactory factory;
  local_service_ = Connect(sync_client, &factory, user_share, error_handler,
                           weak_ptr_factory.GetWeakPtr());
  if (!local_service_.get()) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Failed to connect to syncer.", type_);
    local_merge_result.set_error(error);
    start_done.Run(DataTypeController::ASSOCIATION_FAILED, local_merge_result,
                   syncer_merge_result);
    return;
  }

  if (!CryptoReadyIfNecessary()) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::CRYPTO_ERROR, "",
                            type_);
    local_merge_result.set_error(error);
    start_done.Run(DataTypeController::NEEDS_CRYPTO, local_merge_result,
                   syncer_merge_result);
    return;
  }

  bool sync_has_nodes = false;
  if (!SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::UNRECOVERABLE_ERROR,
                            "Failed to load sync nodes", type_);
    local_merge_result.set_error(error);
    start_done.Run(DataTypeController::UNRECOVERABLE_ERROR, local_merge_result,
                   syncer_merge_result);
    return;
  }

  // Scope for |initial_sync_data| which might be expensive, so we don't want
  // to keep it in memory longer than necessary.
  {
    syncer::SyncDataList initial_sync_data;

    base::TimeTicks start_time = base::TimeTicks::Now();
    syncer::SyncError error =
        GetAllSyncDataReturnError(type_, &initial_sync_data);
    if (error.IsSet()) {
      local_merge_result.set_error(error);
      start_done.Run(DataTypeController::ASSOCIATION_FAILED, local_merge_result,
                     syncer_merge_result);
      return;
    }

    std::string datatype_context;
    if (GetDataTypeContext(&datatype_context)) {
      local_service_->UpdateDataTypeContext(
          type_, syncer::SyncChangeProcessor::NO_REFRESH, datatype_context);
    }

    syncer_merge_result.set_num_items_before_association(
        initial_sync_data.size());
    // Passes a reference to |shared_change_processor|.
    local_merge_result = local_service_->MergeDataAndStartSyncing(
        type_, initial_sync_data, std::unique_ptr<syncer::SyncChangeProcessor>(
                                      new SharedChangeProcessorRef(this)),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new SharedChangeProcessorRef(this)));
    RecordAssociationTime(base::TimeTicks::Now() - start_time);
    if (local_merge_result.error().IsSet()) {
      start_done.Run(DataTypeController::ASSOCIATION_FAILED, local_merge_result,
                     syncer_merge_result);
      return;
    }
  }

  syncer_merge_result.set_num_items_after_association(GetSyncCount());

  start_done.Run(!sync_has_nodes ? DataTypeController::OK_FIRST_RUN
                                 : DataTypeController::OK,
                 local_merge_result, syncer_merge_result);
}

base::WeakPtr<syncer::SyncableService> SharedChangeProcessor::Connect(
    SyncClient* sync_client,
    GenericChangeProcessorFactory* processor_factory,
    syncer::UserShare* user_share,
    syncer::DataTypeErrorHandler* error_handler,
    const base::WeakPtr<syncer::SyncMergeResult>& merge_result) {
  DCHECK(sync_client);
  DCHECK(error_handler);
  backend_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  AutoLock lock(monitor_lock_);
  if (disconnected_)
    return base::WeakPtr<syncer::SyncableService>();
  error_handler_ = error_handler;
  base::WeakPtr<syncer::SyncableService> local_service =
      sync_client->GetSyncableServiceForType(type_);
  if (!local_service.get()) {
    LOG(WARNING) << "SyncableService destroyed before DTC was stopped.";
    disconnected_ = true;
    return base::WeakPtr<syncer::SyncableService>();
  }

  generic_change_processor_ = processor_factory
                                  ->CreateGenericChangeProcessor(
                                      type_, user_share, error_handler,
                                      local_service, merge_result, sync_client)
                                  .release();
  // If available, propagate attachment service to the syncable service.
  std::unique_ptr<syncer::AttachmentService> attachment_service =
      generic_change_processor_->GetAttachmentService();
  if (attachment_service) {
    local_service->SetAttachmentService(std::move(attachment_service));
  }
  return local_service;
}

bool SharedChangeProcessor::Disconnect() {
  // May be called from any thread.
  DVLOG(1) << "Disconnecting change processor.";
  AutoLock lock(monitor_lock_);
  bool was_connected = !disconnected_;
  disconnected_ = true;
  error_handler_ = NULL;
  return was_connected;
}

ChangeProcessor* SharedChangeProcessor::generic_change_processor() {
  return generic_change_processor_;
}

int SharedChangeProcessor::GetSyncCount() {
  DCHECK(backend_task_runner_.get());
  DCHECK(backend_task_runner_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return 0;
  }
  return generic_change_processor_->GetSyncCount();
}

syncer::SyncError SharedChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& list_of_changes) {
  DCHECK(backend_task_runner_.get());
  DCHECK(backend_task_runner_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    // The DTC that disconnects us must ensure it posts a StopSyncing task.
    // If we reach this, it means it just hasn't executed yet.
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Change processor disconnected.", type_);
    return error;
  }
  return generic_change_processor_->ProcessSyncChanges(from_here,
                                                       list_of_changes);
}

syncer::SyncDataList SharedChangeProcessor::GetAllSyncData(
    syncer::ModelType type) const {
  syncer::SyncDataList data;
  GetAllSyncDataReturnError(type, &data);  // Handles the disconnect case.
  return data;
}

syncer::SyncError SharedChangeProcessor::GetAllSyncDataReturnError(
    syncer::ModelType type,
    syncer::SyncDataList* data) const {
  DCHECK(backend_task_runner_.get());
  DCHECK(backend_task_runner_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Change processor disconnected.", type_);
    return error;
  }
  return generic_change_processor_->GetAllSyncDataReturnError(data);
}

syncer::SyncError SharedChangeProcessor::UpdateDataTypeContext(
    syncer::ModelType type,
    syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
    const std::string& context) {
  DCHECK(backend_task_runner_.get());
  DCHECK(backend_task_runner_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Change processor disconnected.", type_);
    return error;
  }
  return generic_change_processor_->UpdateDataTypeContext(type, refresh_status,
                                                          context);
}

bool SharedChangeProcessor::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(backend_task_runner_.get());
  DCHECK(backend_task_runner_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return false;
  }
  return generic_change_processor_->SyncModelHasUserCreatedNodes(has_nodes);
}

bool SharedChangeProcessor::CryptoReadyIfNecessary() {
  DCHECK(backend_task_runner_.get());
  DCHECK(backend_task_runner_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return true;  // Otherwise we get into infinite spin waiting.
  }
  return generic_change_processor_->CryptoReadyIfNecessary();
}

bool SharedChangeProcessor::GetDataTypeContext(std::string* context) const {
  DCHECK(backend_task_runner_.get());
  DCHECK(backend_task_runner_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return false;
  }
  return generic_change_processor_->GetDataTypeContext(context);
}

syncer::SyncError SharedChangeProcessor::CreateAndUploadError(
    const tracked_objects::Location& location,
    const std::string& message) {
  AutoLock lock(monitor_lock_);
  if (!disconnected_) {
    return error_handler_->CreateAndUploadError(location, message, type_);
  } else {
    return syncer::SyncError(location, syncer::SyncError::DATATYPE_ERROR,
                             message, type_);
  }
}

void SharedChangeProcessor::RecordAssociationTime(base::TimeDelta time) {
#define PER_DATA_TYPE_MACRO(type_str) \
  UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type_);
#undef PER_DATA_TYPE_MACRO
}

void SharedChangeProcessor::StopLocalService() {
  if (local_service_.get())
    local_service_->StopSyncing(type_);
  local_service_.reset();
}

}  // namespace sync_driver
