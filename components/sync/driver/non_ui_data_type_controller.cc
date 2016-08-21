// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/non_ui_data_type_controller.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/api/sync_error.h"
#include "components/sync/api/sync_merge_result.h"
#include "components/sync/api/syncable_service.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/generic_change_processor_factory.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"

namespace sync_driver {

SharedChangeProcessor* NonUIDataTypeController::CreateSharedChangeProcessor() {
  return new SharedChangeProcessor(type());
}

NonUIDataTypeController::NonUIDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    SyncClient* sync_client)
    : DirectoryDataTypeController(ui_thread, error_callback),
      sync_client_(sync_client),
      state_(NOT_RUNNING),
      ui_thread_(ui_thread) {}

void NonUIDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  model_load_callback_ = model_load_callback;
  if (state() != NOT_RUNNING) {
    model_load_callback.Run(
        type(), syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                                  "Model already running", type()));
    return;
  }

  state_ = MODEL_STARTING;
  // Since we can't be called multiple times before Stop() is called,
  // |shared_change_processor_| must be NULL here.
  DCHECK(!shared_change_processor_.get());
  shared_change_processor_ = CreateSharedChangeProcessor();
  DCHECK(shared_change_processor_.get());
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
  state_ = MODEL_LOADED;
  model_load_callback_.Run(type(), syncer::SyncError());
}

bool NonUIDataTypeController::StartModels() {
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

void NonUIDataTypeController::StopModels() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
}

void NonUIDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(!start_callback.is_null());
  DCHECK_EQ(state_, MODEL_LOADED);
  state_ = ASSOCIATING;

  // Store UserShare now while on UI thread to avoid potential race
  // condition in StartAssociationWithSharedChangeProcessor.
  DCHECK(sync_client_->GetSyncService());
  user_share_ = sync_client_->GetSyncService()->GetUserShare();

  start_callback_ = start_callback;
  if (!StartAssociationAsync()) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Failed to post StartAssociation", type());
    syncer::SyncMergeResult local_merge_result(type());
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED, local_merge_result,
              syncer::SyncMergeResult(type()));
    // StartDone should have cleared the SharedChangeProcessor.
    DCHECK(!shared_change_processor_.get());
    return;
  }
}

void NonUIDataTypeController::Stop() {
  DCHECK(ui_thread_->BelongsToCurrentThread());

  if (state() == NOT_RUNNING)
    return;

  // Disconnect the change processor. At this point, the
  // syncer::SyncableService can no longer interact with the Syncer, even if
  // it hasn't finished MergeDataAndStartSyncing.
  DisconnectSharedChangeProcessor();

  // If we haven't finished starting, we need to abort the start.
  bool service_started = state() == ASSOCIATING || state() == RUNNING;
  state_ = service_started ? STOPPING : NOT_RUNNING;
  StopModels();

  if (service_started)
    StopSyncableService();

  shared_change_processor_ = nullptr;
  state_ = NOT_RUNNING;
}

std::string NonUIDataTypeController::name() const {
  // For logging only.
  return syncer::ModelTypeToString(type());
}

DataTypeController::State NonUIDataTypeController::state() const {
  return state_;
}

void NonUIDataTypeController::OnSingleDataTypeUnrecoverableError(
    const syncer::SyncError& error) {
  DCHECK(!ui_thread_->BelongsToCurrentThread());
  // TODO(tim): We double-upload some errors.  See bug 383480.
  if (!error_callback_.is_null())
    error_callback_.Run();
  ui_thread_->PostTask(
      error.location(),
      base::Bind(&NonUIDataTypeController::DisableImpl, this, error));
}

NonUIDataTypeController::NonUIDataTypeController()
    : DirectoryDataTypeController(base::ThreadTaskRunnerHandle::Get(),
                                  base::Closure()),
      sync_client_(NULL) {}

NonUIDataTypeController::~NonUIDataTypeController() {}

void NonUIDataTypeController::StartDone(
    DataTypeController::ConfigureResult start_result,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK(ui_thread_->BelongsToCurrentThread());

  DataTypeController::State new_state;
  if (IsSuccessfulResult(start_result)) {
    new_state = RUNNING;
  } else {
    new_state = (start_result == ASSOCIATION_FAILED ? DISABLED : NOT_RUNNING);
  }

  // If we failed to start up, and we haven't been stopped yet, we need to
  // ensure we clean up the local service and shared change processor properly.
  if (new_state != RUNNING && state() != NOT_RUNNING && state() != STOPPING) {
    DisconnectSharedChangeProcessor();
    StopSyncableService();
    shared_change_processor_ = nullptr;
  }

  // It's possible to have StartDone called first from the UI thread
  // (due to Stop being called) and then posted from the non-UI thread. In
  // this case, we drop the second call because we've already been stopped.
  if (state_ == NOT_RUNNING) {
    return;
  }

  state_ = new_state;
  if (state_ != RUNNING) {
    // Start failed.
    StopModels();
    RecordStartFailure(start_result);
  }

  start_callback_.Run(start_result, local_merge_result, syncer_merge_result);
}

void NonUIDataTypeController::RecordStartFailure(ConfigureResult result) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str)                                    \
  UMA_HISTOGRAM_ENUMERATION("Sync." type_str "ConfigureFailure", result, \
                            MAX_CONFIGURE_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void NonUIDataTypeController::DisableImpl(const syncer::SyncError& error) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
  if (!model_load_callback_.is_null()) {
    model_load_callback_.Run(type(), error);
  }
}

bool NonUIDataTypeController::StartAssociationAsync() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK_EQ(state(), ASSOCIATING);
  return PostTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SharedChangeProcessor::StartAssociation,
                 shared_change_processor_,
                 syncer::BindToTaskRunner(
                     ui_thread_,
                     base::Bind(&NonUIDataTypeController::StartDone, this)),
                 sync_client_, user_share_, base::Unretained(this)));
}

ChangeProcessor* NonUIDataTypeController::GetChangeProcessor() const {
  DCHECK_EQ(state_, RUNNING);
  return shared_change_processor_->generic_change_processor();
}

void NonUIDataTypeController::DisconnectSharedChangeProcessor() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  // |shared_change_processor_| can already be NULL if Stop() is
  // called after StartDone(_, DISABLED, _).
  if (shared_change_processor_.get()) {
    shared_change_processor_->Disconnect();
  }
}

void NonUIDataTypeController::StopSyncableService() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  if (shared_change_processor_.get()) {
    PostTaskOnBackendThread(FROM_HERE,
                            base::Bind(&SharedChangeProcessor::StopLocalService,
                                       shared_change_processor_));
  }
}

}  // namespace sync_driver
