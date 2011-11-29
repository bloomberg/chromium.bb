// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/shared_change_processor.h"

#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"

using base::AutoLock;
using content::BrowserThread;

namespace browser_sync {

SharedChangeProcessor::SharedChangeProcessor()
    : disconnected_(false) {
  // We're always created on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DetachFromThread();
}

SharedChangeProcessor::~SharedChangeProcessor() {
  // We can either be deleted when the DTC is destroyed (on UI thread),
  // or when the SyncableService stop's syncing (datatype thread).
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         CalledOnValidThread());
  DetachFromThread();
}

bool SharedChangeProcessor::Connect(
    ProfileSyncComponentsFactory* sync_factory,
    ProfileSyncService* sync_service,
    UnrecoverableErrorHandler* error_handler,
    const base::WeakPtr<SyncableService>& local_service) {
  DCHECK(CalledOnValidThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_)
    return false;
  if (!local_service) {
    NOTREACHED() << "SyncableService destroyed before DTC was stopped.";
    disconnected_ = true;
    return false;
  }
  generic_change_processor_.reset(
      sync_factory->CreateGenericChangeProcessor(sync_service,
                                                 error_handler,
                                                 local_service));
  return true;
}

bool SharedChangeProcessor::Disconnect() {
  // May be called from any thread.
  DVLOG(1) << "Disconnecting change processor.";
  AutoLock lock(monitor_lock_);
  bool was_connected = !disconnected_;
  disconnected_ = true;
  return was_connected;
}

SyncError SharedChangeProcessor::GetSyncDataForType(
    syncable::ModelType type,
    SyncDataList* current_sync_data) {
  DCHECK(CalledOnValidThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    SyncError error(FROM_HERE, "Change processor disconnected.", type);
    return error;
  }
  return generic_change_processor_->GetSyncDataForType(type, current_sync_data);
}

SyncError SharedChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& list_of_changes) {
  DCHECK(CalledOnValidThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    // The DTC that disconnects us must ensure it posts a StopSyncing task.
    // If we reach this, it means it just hasn't executed yet.
    syncable::ModelType type = syncable::UNSPECIFIED;
    if (list_of_changes.size() > 0) {
      type = list_of_changes[0].sync_data().GetDataType();
    }
    SyncError error(FROM_HERE, "Change processor disconnected.", type);
    return error;
  }
  return generic_change_processor_->ProcessSyncChanges(
      from_here, list_of_changes);
}

bool SharedChangeProcessor::SyncModelHasUserCreatedNodes(
    syncable::ModelType type,
    bool* has_nodes) {
  DCHECK(CalledOnValidThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return false;
  }
  return generic_change_processor_->SyncModelHasUserCreatedNodes(
      type, has_nodes);
}

bool SharedChangeProcessor::CryptoReadyIfNecessary(syncable::ModelType type) {
  DCHECK(CalledOnValidThread());
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return true;  // Otherwise we get into infinite spin waiting.
  }
  return generic_change_processor_->CryptoReadyIfNecessary(type);
}

void SharedChangeProcessor::ActivateDataType(
    ProfileSyncService* sync_service,
    syncable::ModelType model_type,
    browser_sync::ModelSafeGroup model_safe_group) {
  AutoLock lock(monitor_lock_);
  if (disconnected_) {
    LOG(ERROR) << "Change processor disconnected.";
    return;
  }
  sync_service->ActivateDataType(
      model_type, model_safe_group, generic_change_processor_.get());
}

}  // namespace browser_sync
