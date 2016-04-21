// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/shared_change_processor.h"

#include <utility>

#include "base/thread_task_runner_handle.h"
#include "components/sync_driver/generic_change_processor.h"
#include "components/sync_driver/generic_change_processor_factory.h"
#include "components/sync_driver/sync_client.h"
#include "sync/api/sync_change.h"
#include "sync/api/syncable_service.h"

using base::AutoLock;

namespace syncer {
class AttachmentService;
}

namespace sync_driver {

SharedChangeProcessor::SharedChangeProcessor()
    : disconnected_(false),
      type_(syncer::UNSPECIFIED),
      frontend_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      generic_change_processor_(NULL),
      error_handler_(NULL) {
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

base::WeakPtr<syncer::SyncableService> SharedChangeProcessor::Connect(
    SyncClient* sync_client,
    GenericChangeProcessorFactory* processor_factory,
    syncer::UserShare* user_share,
    DataTypeErrorHandler* error_handler,
    syncer::ModelType type,
    const base::WeakPtr<syncer::SyncMergeResult>& merge_result) {
  DCHECK(sync_client);
  DCHECK(error_handler);
  DCHECK_NE(type, syncer::UNSPECIFIED);
  backend_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  AutoLock lock(monitor_lock_);
  if (disconnected_)
    return base::WeakPtr<syncer::SyncableService>();
  type_ = type;
  error_handler_ = error_handler;
  base::WeakPtr<syncer::SyncableService> local_service =
      sync_client->GetSyncableServiceForType(type);
  if (!local_service.get()) {
    LOG(WARNING) << "SyncableService destroyed before DTC was stopped.";
    disconnected_ = true;
    return base::WeakPtr<syncer::SyncableService>();
  }

  generic_change_processor_ =
      processor_factory->CreateGenericChangeProcessor(type,
                                                      user_share,
                                                      error_handler,
                                                      local_service,
                                                      merge_result,
                                                      sync_client).release();
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
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Change processor disconnected.",
                            type_);
    return error;
  }
  return generic_change_processor_->ProcessSyncChanges(
      from_here, list_of_changes);
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
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Change processor disconnected.",
                            type_);
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
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Change processor disconnected.",
                            type_);
    return error;
  }
  return generic_change_processor_->UpdateDataTypeContext(
      type, refresh_status, context);
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
    return syncer::SyncError(location,
                             syncer::SyncError::DATATYPE_ERROR,
                             message,
                             type_);
  }
}

}  // namespace sync_driver
