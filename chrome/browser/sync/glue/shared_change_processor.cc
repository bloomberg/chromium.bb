// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/shared_change_processor.h"

#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_change.h"

using base::AutoLock;
using content::BrowserThread;

namespace browser_sync {

SharedChangeProcessor::SharedChangeProcessor()
    : disconnected_(false),
      type_(syncer::UNSPECIFIED),
      sync_service_(NULL),
      generic_change_processor_(NULL),
      error_handler_(NULL) {
  // We're always created on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

SharedChangeProcessor::~SharedChangeProcessor() {
  // We can either be deleted when the DTC is destroyed (on UI
  // thread), or when the syncer::SyncableService stop's syncing (datatype
  // thread).  |generic_change_processor_|, if non-NULL, must be
  // deleted on |backend_loop_|.
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    if (backend_loop_.get()) {
      if (!backend_loop_->DeleteSoon(FROM_HERE, generic_change_processor_)) {
        NOTREACHED();
      }
    } else {
      DCHECK(!generic_change_processor_);
    }
  } else {
    DCHECK(backend_loop_.get());
    DCHECK(backend_loop_->BelongsToCurrentThread());
    delete generic_change_processor_;
  }
}

base::WeakPtr<syncer::SyncableService> SharedChangeProcessor::Connect(
    ProfileSyncComponentsFactory* sync_factory,
    ProfileSyncService* sync_service,
    DataTypeErrorHandler* error_handler,
    syncer::ModelType type,
    const base::WeakPtr<syncer::SyncMergeResult>& merge_result) {
  DCHECK(sync_factory);
  DCHECK(sync_service);
  DCHECK(error_handler);
  DCHECK_NE(type, syncer::UNSPECIFIED);
  backend_loop_ = base::MessageLoopProxy::current();
  AutoLock lock(monitor_lock_);
  if (disconnected_)
    return base::WeakPtr<syncer::SyncableService>();
  type_ = type;
  sync_service_ = sync_service;
  error_handler_ = error_handler;
  base::WeakPtr<syncer::SyncableService> local_service =
      sync_factory->GetSyncableServiceForType(type);
  if (!local_service.get()) {
    LOG(WARNING) << "SyncableService destroyed before DTC was stopped.";
    disconnected_ = true;
    return base::WeakPtr<syncer::SyncableService>();
  }

  // TODO(zea): Pass |merge_result| to the generic change processor.
  generic_change_processor_ =
      sync_factory->CreateGenericChangeProcessor(sync_service_,
                                                 error_handler,
                                                 local_service,
                                                 merge_result);
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

int SharedChangeProcessor::GetSyncCount() {
  DCHECK(backend_loop_.get());
  DCHECK(backend_loop_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return 0;
  }
  return generic_change_processor_->GetSyncCountForType(type_);
}

syncer::SyncError SharedChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& list_of_changes) {
  DCHECK(backend_loop_.get());
  DCHECK(backend_loop_->BelongsToCurrentThread());
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
  DCHECK(backend_loop_.get());
  DCHECK(backend_loop_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Change processor disconnected.",
                            type_);
    return error;
  }
  return generic_change_processor_->GetAllSyncDataReturnError(type, data);
}

syncer::SyncError SharedChangeProcessor::UpdateDataTypeContext(
    syncer::ModelType type,
    syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
    const std::string& context) {
  DCHECK(backend_loop_.get());
  DCHECK(backend_loop_->BelongsToCurrentThread());
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
  DCHECK(backend_loop_.get());
  DCHECK(backend_loop_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return false;
  }
  return generic_change_processor_->SyncModelHasUserCreatedNodes(
      type_, has_nodes);
}

bool SharedChangeProcessor::CryptoReadyIfNecessary() {
  DCHECK(backend_loop_.get());
  DCHECK(backend_loop_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return true;  // Otherwise we get into infinite spin waiting.
  }
  return generic_change_processor_->CryptoReadyIfNecessary(type_);
}

void SharedChangeProcessor::ActivateDataType(
    syncer::ModelSafeGroup model_safe_group) {
  DCHECK(backend_loop_.get());
  DCHECK(backend_loop_->BelongsToCurrentThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return;
  }
  sync_service_->ActivateDataType(type_,
                                  model_safe_group,
                                  generic_change_processor_);
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

}  // namespace browser_sync
