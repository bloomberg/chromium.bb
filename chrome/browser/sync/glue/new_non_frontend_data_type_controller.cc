// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/new_non_frontend_data_type_controller.h"

#include "base/logging.h"
#include "chrome/browser/sync/glue/shared_change_processor_ref.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"
#include "sync/syncable/model_type.h"

using content::BrowserThread;

namespace browser_sync {

NewNonFrontendDataTypeController::NewNonFrontendDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : NonFrontendDataTypeController(profile_sync_factory,
                                    profile,
                                    sync_service) {}

void NewNonFrontendDataTypeController::Start(
    const StartCallback& start_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!start_callback.is_null());
  if (state() != NOT_RUNNING) {
    start_callback.Run(BUSY, SyncError());
    return;
  }

  set_start_callback(start_callback);

  // Since we can't be called multiple times before Stop() is called,
  // |shared_change_processor_| must be NULL here.
  DCHECK(!shared_change_processor_.get());
  shared_change_processor_ =
      profile_sync_factory()->CreateSharedChangeProcessor();
  DCHECK(shared_change_processor_.get());

  set_state(MODEL_STARTING);
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early.
    DCHECK(state() == MODEL_STARTING || state() == NOT_RUNNING);
    return;
  }

  // Kick off association on the thread the datatype resides on.
  set_state(ASSOCIATING);
  if (!StartAssociationAsync()) {
    SyncError error(FROM_HERE, "Failed to post StartAssociation", type());
    StartDoneImpl(ASSOCIATION_FAILED, NOT_RUNNING, error);
    // StartDoneImpl should have called ClearSharedChangeProcessor();
    DCHECK(!shared_change_processor_.get());
    return;
  }
}

void NewNonFrontendDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state() == NOT_RUNNING) {
    // Stop() should never be called for datatypes that are already stopped.
    NOTREACHED();
    return;
  }

  // Disconnect the change processor. At this point, the
  // SyncableService can no longer interact with the Syncer, even if
  // it hasn't finished MergeDataAndStartSyncing.
  ClearSharedChangeProcessor();

  // If we haven't finished starting, we need to abort the start.
  switch (state()) {
    case MODEL_STARTING:
      set_state(STOPPING);
      StartDoneImpl(ABORTED, NOT_RUNNING, SyncError());
      return;  // The datatype was never activated, we're done.
    case ASSOCIATING:
      set_state(STOPPING);
      StartDoneImpl(ABORTED, NOT_RUNNING, SyncError());
      // We continue on to deactivate the datatype and stop the local service.
      break;
    case DISABLED:
      // If we're disabled we never succeded associating and never activated the
      // datatype. We would have already stopped the local service in
      // StartDoneImpl(..).
      set_state(NOT_RUNNING);
      StopModels();
      return;
    default:
      // Datatype was fully started. Need to deactivate and stop the local
      // service.
      DCHECK_EQ(state(), RUNNING);
      set_state(STOPPING);
      StopModels();
      break;
  }

  // Deactivate the DataType on the UI thread. We dont want to listen
  // for any more changes or process them from the server.
  profile_sync_service()->DeactivateDataType(type());

  // Stop the local service and release our references to it and the
  // shared change processor (posts a task to the datatype's thread).
  StopLocalServiceAsync();

  set_state(NOT_RUNNING);
}

NewNonFrontendDataTypeController::NewNonFrontendDataTypeController() {}

NewNonFrontendDataTypeController::~NewNonFrontendDataTypeController() {}

void NewNonFrontendDataTypeController::StartDone(
    DataTypeController::StartResult result,
    DataTypeController::State new_state,
    const SyncError& error) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(
          &NewNonFrontendDataTypeController::StartDoneImpl,
          this,
          result,
          new_state,
          error));
}

void NewNonFrontendDataTypeController::StartDoneImpl(
    DataTypeController::StartResult result,
    DataTypeController::State new_state,
    const SyncError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If we failed to start up, and we haven't been stopped yet, we need to
  // ensure we clean up the local service and shared change processor properly.
  if (new_state != RUNNING && state() != NOT_RUNNING && state() != STOPPING) {
    ClearSharedChangeProcessor();
    StopLocalServiceAsync();
  }
  NonFrontendDataTypeController::StartDoneImpl(result, new_state, error);
}

bool NewNonFrontendDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  return PostTaskOnBackendThread(
      FROM_HERE,
      base::Bind(
          &NewNonFrontendDataTypeController::
              StartAssociationWithSharedChangeProcessor,
          this,
          shared_change_processor_));
}

// This method can execute after we've already stopped (and possibly even
// destroyed) both the Syncer and the SyncableService. As a result, all actions
// must either have no side effects outside of the DTC or must be protected
// by |shared_change_processor|, which is guaranteed to have been Disconnected
// if the syncer shut down.
void NewNonFrontendDataTypeController::
    StartAssociationWithSharedChangeProcessor(
        const scoped_refptr<SharedChangeProcessor>& shared_change_processor) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(shared_change_processor.get());

  // Connect |shared_change_processor| to the syncer and get the
  // SyncableService associated with type().
  // Note that it's possible the shared_change_processor has already been
  // disconnected at this point, so all our accesses to the syncer from this
  // point on are through it.
  local_service_ = shared_change_processor->Connect(profile_sync_factory(),
                                                    profile_sync_service(),
                                                    this,
                                                    type());
  if (!local_service_.get()) {
    SyncError error(FROM_HERE, "Failed to connect to syncer.", type());
    StartFailed(UNRECOVERABLE_ERROR, error);
    return;
  }

  if (!shared_change_processor->CryptoReadyIfNecessary()) {
    StartFailed(NEEDS_CRYPTO, SyncError());
    return;
  }

  bool sync_has_nodes = false;
  if (!shared_change_processor->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    SyncError error(FROM_HERE, "Failed to load sync nodes", type());
    StartFailed(UNRECOVERABLE_ERROR, error);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  SyncError error;
  SyncDataList initial_sync_data;
  error = shared_change_processor->GetSyncData(&initial_sync_data);
  if (error.IsSet()) {
    StartFailed(ASSOCIATION_FAILED, error);
    return;
  }
  // Passes a reference to |shared_change_processor|.
  error = local_service_->MergeDataAndStartSyncing(
      type(),
      initial_sync_data,
      scoped_ptr<SyncChangeProcessor>(
          new SharedChangeProcessorRef(shared_change_processor)),
      scoped_ptr<SyncErrorFactory>(
          new SharedChangeProcessorRef(shared_change_processor)));
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (error.IsSet()) {
    StartFailed(ASSOCIATION_FAILED, error);
    return;
  }

  // If we've been disconnected, profile_sync_service() may return an invalid
  // pointer, but |shared_change_processor| protects us from attempting to
  // access it.
  // Note: This must be done on the datatype's thread to ensure local_service_
  // doesn't start trying to push changes from its thread before we activate
  // the datatype.
  shared_change_processor->ActivateDataType(model_safe_group());
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK, RUNNING, SyncError());
}

void NewNonFrontendDataTypeController::ClearSharedChangeProcessor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // |shared_change_processor_| can already be NULL if Stop() is
  // called after StartDoneImpl(_, DISABLED, _).
  if (shared_change_processor_.get()) {
    shared_change_processor_->Disconnect();
    shared_change_processor_ = NULL;
  }
}

void NewNonFrontendDataTypeController::StopLocalServiceAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PostTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&NewNonFrontendDataTypeController::StopLocalService, this));
}

void NewNonFrontendDataTypeController::StopLocalService() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (local_service_.get())
    local_service_->StopSyncing(type());
  local_service_.reset();
}

void NewNonFrontendDataTypeController::CreateSyncComponents() {
  NOTIMPLEMENTED();
}

}  // namepsace browser_sync
