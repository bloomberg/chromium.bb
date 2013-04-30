// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/non_ui_data_type_controller.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/shared_change_processor_ref.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/util/data_type_histogram.h"

using content::BrowserThread;

namespace browser_sync {

NonUIDataTypeController::NonUIDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      sync_service_(sync_service),
      state_(NOT_RUNNING) {
}

void NonUIDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!model_load_callback.is_null());
  if (state() != NOT_RUNNING) {
    model_load_callback.Run(type(), syncer::SyncError(FROM_HERE,
                                                      "Model already running",
                                                      type()));
    return;
  }

  state_ = MODEL_STARTING;

  // Since we can't be called multiple times before Stop() is called,
  // |shared_change_processor_| must be NULL here.
  DCHECK(!shared_change_processor_.get());
  shared_change_processor_ =
      profile_sync_factory_->CreateSharedChangeProcessor();
  DCHECK(shared_change_processor_.get());

  model_load_callback_ = model_load_callback;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early.
    DCHECK(state() == MODEL_STARTING || state() == NOT_RUNNING);
    return;
  }

  OnModelLoaded();
}

void NonUIDataTypeController::OnModelLoaded() {
  DCHECK_EQ(state_, MODEL_STARTING);
  DCHECK(!model_load_callback_.is_null());
  state_ = MODEL_LOADED;

  ModelLoadCallback model_load_callback = model_load_callback_;
  model_load_callback_.Reset();
  model_load_callback.Run(type(), syncer::SyncError());
}

bool NonUIDataTypeController::StartModels() {
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

void NonUIDataTypeController::StopModels() {
  // Do nothing by default.
}

void NonUIDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!start_callback.is_null());
  DCHECK_EQ(state_, MODEL_LOADED);
  state_ = ASSOCIATING;

  start_callback_ = start_callback;
  if (!StartAssociationAsync()) {
    syncer::SyncError error(
        FROM_HERE, "Failed to post StartAssociation", type());
    syncer::SyncMergeResult local_merge_result(type());
    local_merge_result.set_error(error);
    StartDoneImpl(ASSOCIATION_FAILED,
                  NOT_RUNNING,
                  local_merge_result,
                  syncer::SyncMergeResult(type()));
    // StartDoneImpl should have called ClearSharedChangeProcessor();
    DCHECK(!shared_change_processor_.get());
    return;
  }
}

void NonUIDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state() == NOT_RUNNING) {
    // Stop() should never be called for datatypes that are already stopped.
    NOTREACHED();
    return;
  }

  // Disconnect the change processor. At this point, the
  // syncer::SyncableService can no longer interact with the Syncer, even if
  // it hasn't finished MergeDataAndStartSyncing.
  ClearSharedChangeProcessor();

  // If we haven't finished starting, we need to abort the start.
  switch (state()) {
    case MODEL_STARTING:
      state_ = STOPPING;
      AbortModelLoad();
      return;  // The datatype was never activated, we're done.
    case ASSOCIATING:
      state_ = STOPPING;
      StartDoneImpl(ABORTED,
                    NOT_RUNNING,
                    syncer::SyncMergeResult(type()),
                    syncer::SyncMergeResult(type()));
      // We continue on to deactivate the datatype and stop the local service.
      break;
    case DISABLED:
      // If we're disabled we never succeded associating and never activated the
      // datatype. We would have already stopped the local service in
      // StartDoneImpl(..).
      state_ = NOT_RUNNING;
      StopModels();
      return;
    default:
      // Datatype was fully started. Need to deactivate and stop the local
      // service.
      DCHECK_EQ(state(), RUNNING);
      state_ = STOPPING;
      StopModels();
      break;
  }

  // Deactivate the DataType on the UI thread. We dont want to listen
  // for any more changes or process them from the server.
  sync_service_->DeactivateDataType(type());

  // Stop the local service and release our references to it and the
  // shared change processor (posts a task to the datatype's thread).
  StopLocalServiceAsync();

  state_ = NOT_RUNNING;
}

std::string NonUIDataTypeController::name() const {
  // For logging only.
  return syncer::ModelTypeToString(type());
}

DataTypeController::State NonUIDataTypeController::state() const {
  return state_;
}

void NonUIDataTypeController::OnSingleDatatypeUnrecoverableError(
    const tracked_objects::Location& from_here, const std::string& message) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  RecordUnrecoverableError(from_here, message);
  BrowserThread::PostTask(BrowserThread::UI, from_here,
      base::Bind(&NonUIDataTypeController::DisableImpl,
                 this,
                 from_here,
                 message));
}

NonUIDataTypeController::NonUIDataTypeController()
    : profile_sync_factory_(NULL),
      profile_(NULL),
      sync_service_(NULL) {}

NonUIDataTypeController::~NonUIDataTypeController() {}

void NonUIDataTypeController::StartDone(
    DataTypeController::StartResult start_result,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  DataTypeController::State new_state;
  if (IsSuccessfulResult(start_result)) {
    new_state = RUNNING;
  } else {
    new_state = (start_result == ASSOCIATION_FAILED ? DISABLED : NOT_RUNNING);
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&NonUIDataTypeController::StartDoneImpl,
                 this,
                 start_result,
                 new_state,
                 local_merge_result,
                 syncer_merge_result));
}

void NonUIDataTypeController::StartDoneImpl(
    DataTypeController::StartResult start_result,
    DataTypeController::State new_state,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (IsUnrecoverableResult(start_result))
    RecordUnrecoverableError(FROM_HERE, "StartFailed");

  // If we failed to start up, and we haven't been stopped yet, we need to
  // ensure we clean up the local service and shared change processor properly.
  if (new_state != RUNNING && state() != NOT_RUNNING && state() != STOPPING) {
    ClearSharedChangeProcessor();
    StopLocalServiceAsync();
  }

  // It's possible to have StartDoneImpl called first from the UI thread
  // (due to Stop being called) and then posted from the non-UI thread. In
  // this case, we drop the second call because we've already been stopped.
  if (state_ == NOT_RUNNING) {
    DCHECK(start_callback_.is_null());
    return;
  }

  state_ = new_state;
  if (state_ != RUNNING) {
    // Start failed.
    StopModels();
    RecordStartFailure(start_result);
  }

  // We have to release the callback before we call it, since it's possible
  // invoking the callback will trigger a call to STOP(), which will get
  // confused by the non-NULL start_callback_.
  StartCallback callback = start_callback_;
  start_callback_.Reset();
  callback.Run(start_result, local_merge_result, syncer_merge_result);
}

void NonUIDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void NonUIDataTypeController::RecordStartFailure(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_ENUMERATION("Sync." type_str "StartFailure", result, \
                              MAX_START_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void NonUIDataTypeController::AbortModelLoad() {
  state_ = NOT_RUNNING;
  StopModels();
  ModelLoadCallback model_load_callback = model_load_callback_;
  model_load_callback_.Reset();
  model_load_callback.Run(type(), syncer::SyncError(FROM_HERE,
                                                    "ABORTED",
                                                    type()));
}

void NonUIDataTypeController::DisableImpl(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->DisableBrokenDatatype(type(), from_here, message);
}

bool NonUIDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  return PostTaskOnBackendThread(
      FROM_HERE,
      base::Bind(
          &NonUIDataTypeController::StartAssociationWithSharedChangeProcessor,
          this,
          shared_change_processor_));
}

// This method can execute after we've already stopped (and possibly even
// destroyed) both the Syncer and the SyncableService. As a result, all actions
// must either have no side effects outside of the DTC or must be protected
// by |shared_change_processor|, which is guaranteed to have been Disconnected
// if the syncer shut down.
void NonUIDataTypeController::
    StartAssociationWithSharedChangeProcessor(
        const scoped_refptr<SharedChangeProcessor>& shared_change_processor) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(shared_change_processor.get());
  syncer::SyncMergeResult local_merge_result(type());
  syncer::SyncMergeResult syncer_merge_result(type());
  base::WeakPtrFactory<syncer::SyncMergeResult> weak_ptr_factory(
      &syncer_merge_result);

  // Connect |shared_change_processor| to the syncer and get the
  // syncer::SyncableService associated with type().
  // Note that it's possible the shared_change_processor has already been
  // disconnected at this point, so all our accesses to the syncer from this
  // point on are through it.
  local_service_ = shared_change_processor->Connect(
      profile_sync_factory_,
      sync_service_,
      this,
      type(),
      weak_ptr_factory.GetWeakPtr());
  if (!local_service_) {
    syncer::SyncError error(FROM_HERE, "Failed to connect to syncer.", type());
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  if (!shared_change_processor->CryptoReadyIfNecessary()) {
    StartDone(NEEDS_CRYPTO,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  bool sync_has_nodes = false;
  if (!shared_change_processor->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    syncer::SyncError error(FROM_HERE, "Failed to load sync nodes", type());
    local_merge_result.set_error(error);
    StartDone(UNRECOVERABLE_ERROR,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  syncer::SyncDataList initial_sync_data;
  syncer::SyncError error =
      shared_change_processor->GetSyncData(&initial_sync_data);
  if (error.IsSet()) {
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  syncer_merge_result.set_num_items_before_association(
      initial_sync_data.size());
  // Passes a reference to |shared_change_processor|.
  local_merge_result =
      local_service_->MergeDataAndStartSyncing(
          type(),
          initial_sync_data,
          scoped_ptr<syncer::SyncChangeProcessor>(
              new SharedChangeProcessorRef(shared_change_processor)),
          scoped_ptr<syncer::SyncErrorFactory>(
              new SharedChangeProcessorRef(shared_change_processor)));
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (local_merge_result.error().IsSet()) {
    StartDone(ASSOCIATION_FAILED,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  syncer_merge_result.set_num_items_after_association(
      shared_change_processor->GetSyncCount());

  // If we've been disconnected, sync_service_ may return an invalid
  // pointer, but |shared_change_processor| protects us from attempting to
  // access it.
  // Note: This must be done on the datatype's thread to ensure local_service_
  // doesn't start trying to push changes from its thread before we activate
  // the datatype.
  shared_change_processor->ActivateDataType(model_safe_group());
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK,
            local_merge_result,
            syncer_merge_result);
}

void NonUIDataTypeController::ClearSharedChangeProcessor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // |shared_change_processor_| can already be NULL if Stop() is
  // called after StartDoneImpl(_, DISABLED, _).
  if (shared_change_processor_) {
    shared_change_processor_->Disconnect();
    shared_change_processor_ = NULL;
  }
}

void NonUIDataTypeController::StopLocalServiceAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PostTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&NonUIDataTypeController::StopLocalService, this));
}

void NonUIDataTypeController::StopLocalService() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (local_service_)
    local_service_->StopSyncing(type());
  local_service_.reset();
}

}  // namepsace browser_sync
