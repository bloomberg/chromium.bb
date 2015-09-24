// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/frontend_data_type_controller.h"

#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/model_associator.h"
#include "components/sync_driver/sync_client.h"
#include "components/sync_driver/sync_service.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/util/data_type_histogram.h"

namespace browser_sync {

FrontendDataTypeController::FrontendDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client)
    : DirectoryDataTypeController(ui_thread, error_callback),
      sync_client_(sync_client),
      state_(NOT_RUNNING) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sync_client);
}

void FrontendDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  model_load_callback_ = model_load_callback;

  if (state_ != NOT_RUNNING) {
    model_load_callback.Run(type(),
                            syncer::SyncError(FROM_HERE,
                                              syncer::SyncError::DATATYPE_ERROR,
                                              "Model already running",
                                              type()));
    return;
  }

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early. state_ will control
    // what we perform next.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING);
    return;
  }

  OnModelLoaded();
}

void FrontendDataTypeController::OnModelLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, MODEL_STARTING);

  state_ = MODEL_LOADED;
  model_load_callback_.Run(type(), syncer::SyncError());
}

void FrontendDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!start_callback.is_null());
  DCHECK_EQ(state_, MODEL_LOADED);

  start_callback_ = start_callback;
  state_ = ASSOCIATING;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
    FROM_HERE, base::Bind(&FrontendDataTypeController::Associate, this));
}

void FrontendDataTypeController::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == NOT_RUNNING)
    return;

  State prev_state = state_;
  state_ = STOPPING;

  // If Stop() is called while Start() is waiting for the datatype model to
  // load, abort the start.
  if (prev_state == MODEL_STARTING) {
    AbortModelLoad();
    // We can just return here since we haven't performed association if we're
    // still in MODEL_STARTING.
    return;
  }

  CleanUpState();

  if (model_associator()) {
    syncer::SyncError error;  // Not used.
    error = model_associator()->DisassociateModels();
  }

  set_model_associator(NULL);
  change_processor_.reset();

  state_ = NOT_RUNNING;
}

syncer::ModelSafeGroup FrontendDataTypeController::model_safe_group()
    const {
  return syncer::GROUP_UI;
}

std::string FrontendDataTypeController::name() const {
  // For logging only.
  return syncer::ModelTypeToString(type());
}

sync_driver::DataTypeController::State FrontendDataTypeController::state()
    const {
  return state_;
}

void FrontendDataTypeController::OnSingleDataTypeUnrecoverableError(
    const syncer::SyncError& error) {
  DCHECK_EQ(type(), error.model_type());
  RecordUnrecoverableError(error.location(), error.message());
  if (!model_load_callback_.is_null()) {
    syncer::SyncMergeResult local_merge_result(type());
    local_merge_result.set_error(error);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(model_load_callback_, type(), error));
  }
}

FrontendDataTypeController::FrontendDataTypeController()
    : DirectoryDataTypeController(base::ThreadTaskRunnerHandle::Get(),
                                  base::Closure()),
      sync_client_(NULL),
      state_(NOT_RUNNING) {}

FrontendDataTypeController::~FrontendDataTypeController() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool FrontendDataTypeController::StartModels() {
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

void FrontendDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DVLOG(1) << "Datatype Controller failed for type "
           << ModelTypeToString(type()) << "  "
           << message << " at location "
           << from_here.ToString();
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);

  if (!error_callback_.is_null())
    error_callback_.Run();
}

void FrontendDataTypeController::Associate() {
  if (state_ != ASSOCIATING) {
    // Stop() must have been called while Associate() task have been waiting.
    DCHECK_EQ(state_, NOT_RUNNING);
    return;
  }

  syncer::SyncMergeResult local_merge_result(type());
  syncer::SyncMergeResult syncer_merge_result(type());
  CreateSyncComponents();
  if (!model_associator()->CryptoReadyIfNecessary()) {
    StartDone(NEEDS_CRYPTO, local_merge_result, syncer_merge_result);
    return;
  }

  bool sync_has_nodes = false;
  if (!model_associator()->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::UNRECOVERABLE_ERROR,
                            "Failed to load sync nodes",
                            type());
    local_merge_result.set_error(error);
    StartDone(UNRECOVERABLE_ERROR, local_merge_result, syncer_merge_result);
    return;
  }

  // TODO(zea): Have AssociateModels fill the local and syncer merge results.
  base::TimeTicks start_time = base::TimeTicks::Now();
  syncer::SyncError error;
  error = model_associator()->AssociateModels(
      &local_merge_result,
      &syncer_merge_result);
  // TODO(lipalani): crbug.com/122690 - handle abort.
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (error.IsSet()) {
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED, local_merge_result, syncer_merge_result);
    return;
  }

  state_ = RUNNING;
  // FinishStart() invokes the DataTypeManager callback, which can lead to a
  // call to Stop() if one of the other data types being started generates an
  // error.
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK,
            local_merge_result,
            syncer_merge_result);
}

void FrontendDataTypeController::CleanUpState() {
  // Do nothing by default.
}

void FrontendDataTypeController::CleanUp() {
  CleanUpState();
  set_model_associator(NULL);
  change_processor_.reset();
}

void FrontendDataTypeController::AbortModelLoad() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CleanUp();
  state_ = NOT_RUNNING;
}

void FrontendDataTypeController::StartDone(
    ConfigureResult start_result,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsSuccessfulResult(start_result)) {
    if (IsUnrecoverableResult(start_result))
      RecordUnrecoverableError(FROM_HERE, "StartFailed");

    CleanUp();
    if (start_result == ASSOCIATION_FAILED) {
      state_ = DISABLED;
    } else {
      state_ = NOT_RUNNING;
    }
    RecordStartFailure(start_result);
  }

  start_callback_.Run(start_result, local_merge_result, syncer_merge_result);
}

void FrontendDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  DCHECK(thread_checker_.CalledOnValidThread());
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void FrontendDataTypeController::RecordStartFailure(ConfigureResult result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str)                                    \
  UMA_HISTOGRAM_ENUMERATION("Sync." type_str "ConfigureFailure", result, \
                            MAX_CONFIGURE_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

sync_driver::AssociatorInterface* FrontendDataTypeController::model_associator()
    const {
  return model_associator_.get();
}

void FrontendDataTypeController::set_model_associator(
    sync_driver::AssociatorInterface* model_associator) {
  model_associator_.reset(model_associator);
}

sync_driver::ChangeProcessor* FrontendDataTypeController::GetChangeProcessor()
    const {
  return change_processor_.get();
}

void FrontendDataTypeController::set_change_processor(
    sync_driver::ChangeProcessor* change_processor) {
  change_processor_.reset(change_processor);
}

}  // namespace browser_sync
