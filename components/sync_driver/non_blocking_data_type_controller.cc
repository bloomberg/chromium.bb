// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/non_blocking_data_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "components/sync_driver/backend_data_type_configurer.h"
#include "components/sync_driver/sync_client.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/shared_model_type_processor.h"
#include "sync/util/data_type_histogram.h"

namespace sync_driver_v2 {

NonBlockingDataTypeController::NonBlockingDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    syncer::ModelType model_type,
    sync_driver::SyncClient* sync_client)
    : sync_driver::DataTypeController(ui_thread, error_callback),
      model_type_(model_type),
      sync_client_(sync_client),
      state_(NOT_RUNNING) {
  // TODO(gangwu): should initial processor somewhere else in
  // NonBlockingDataTypeController
  InitializeProcessor();
}

NonBlockingDataTypeController::~NonBlockingDataTypeController() {}

void NonBlockingDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  DCHECK(!model_load_callback.is_null());
  model_load_callback_ = model_load_callback;

  if (state() != NOT_RUNNING) {
    LoadModelsDone(
        RUNTIME_ERROR,
        syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                          "Model already running", type()));
    return;
  }

  state_ = MODEL_STARTING;

  // Start the type processor on the model thread.
  if (!RunOnModelThread(
          FROM_HERE,
          base::Bind(
              &syncer_v2::SharedModelTypeProcessor::OnSyncStarting,
              type_processor(),
              base::Bind(&NonBlockingDataTypeController::OnProcessorStarted,
                         this)))) {
    LoadModelsDone(
        UNRECOVERABLE_ERROR,
        syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                          "Failed to post model Start", type()));
  }
}

void NonBlockingDataTypeController::LoadModelsDone(
    ConfigureResult result,
    const syncer::SyncError& error) {
  DCHECK(BelongsToUIThread());

  if (state_ == NOT_RUNNING) {
    // The callback arrived on the UI thread after the type has been already
    // stopped.
    RecordStartFailure(ABORTED);
    return;
  }

  if (IsSuccessfulResult(result)) {
    DCHECK_EQ(MODEL_STARTING, state_);
    state_ = MODEL_LOADED;
  } else {
    RecordStartFailure(result);
  }

  if (!model_load_callback_.is_null()) {
    model_load_callback_.Run(type(), error);
  }
}

void NonBlockingDataTypeController::OnProcessorStarted(
    syncer::SyncError error,
    scoped_ptr<syncer_v2::ActivationContext> activation_context) {
  if (BelongsToUIThread()) {
    // Hold on to the activation context until ActivateDataType is called.
    if (state_ == MODEL_STARTING) {
      activation_context_ = std::move(activation_context);
    }
    // TODO(stanisc): Figure out if UNRECOVERABLE_ERROR is OK in this case.
    ConfigureResult result = error.IsSet() ? UNRECOVERABLE_ERROR : OK;
    LoadModelsDone(result, error);
  } else {
    RunOnUIThread(
        FROM_HERE,
        base::Bind(&NonBlockingDataTypeController::OnProcessorStarted, this,
                   error, base::Passed(std::move(activation_context))));
  }
}

void NonBlockingDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(BelongsToUIThread());
  DCHECK(!start_callback.is_null());

  state_ = RUNNING;

  // There is no association, just call back promptly.
  syncer::SyncMergeResult merge_result(type());
  start_callback.Run(OK, merge_result, merge_result);
}

void NonBlockingDataTypeController::ActivateDataType(
    sync_driver::BackendDataTypeConfigurer* configurer) {
  DCHECK(BelongsToUIThread());
  DCHECK(configurer);
  DCHECK(activation_context_);
  DCHECK_EQ(RUNNING, state_);
  configurer->ActivateNonBlockingDataType(type(),
                                          std::move(activation_context_));
}

void NonBlockingDataTypeController::DeactivateDataType(
    sync_driver::BackendDataTypeConfigurer* configurer) {
  DCHECK(BelongsToUIThread());
  DCHECK(configurer);
  configurer->DeactivateNonBlockingDataType(type());
}

void NonBlockingDataTypeController::Stop() {
  DCHECK(ui_thread()->BelongsToCurrentThread());

  if (state() == NOT_RUNNING)
    return;

  state_ = NOT_RUNNING;

  RunOnModelThread(
      FROM_HERE,
      base::Bind(&syncer_v2::SharedModelTypeProcessor::DisconnectSync,
                 type_processor()));
}

std::string NonBlockingDataTypeController::name() const {
  // For logging only.
  return syncer::ModelTypeToString(type());
}

sync_driver::DataTypeController::State NonBlockingDataTypeController::state()
    const {
  return state_;
}

bool NonBlockingDataTypeController::BelongsToUIThread() const {
  return ui_thread()->BelongsToCurrentThread();
}

void NonBlockingDataTypeController::RunOnUIThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(!BelongsToUIThread());
  ui_thread()->PostTask(from_here, task);
}

void NonBlockingDataTypeController::OnSingleDataTypeUnrecoverableError(
    const syncer::SyncError& error) {
  RecordUnrecoverableError();
  if (!error_callback_.is_null())
    error_callback_.Run();

  ReportLoadModelError(UNRECOVERABLE_ERROR, error);
}

void NonBlockingDataTypeController::ReportLoadModelError(
    ConfigureResult result,
    const syncer::SyncError& error) {
  DCHECK(!IsSuccessfulResult(result));
  if (BelongsToUIThread()) {
    // Report the error only if the model is starting.
    if (state_ == MODEL_STARTING) {
      LoadModelsDone(result, error);
    }
  } else {
    RunOnUIThread(
        error.location(),
        base::Bind(&NonBlockingDataTypeController::ReportLoadModelError, this,
                   result, error));
  }
}

void NonBlockingDataTypeController::RecordStartFailure(
    ConfigureResult result) const {
  DCHECK(BelongsToUIThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str)                                    \
  UMA_HISTOGRAM_ENUMERATION("Sync." type_str "ConfigureFailure", result, \
                            MAX_CONFIGURE_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void NonBlockingDataTypeController::RecordUnrecoverableError() {
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
}

base::WeakPtr<syncer_v2::SharedModelTypeProcessor>
NonBlockingDataTypeController::type_processor() const {
  return type_processor_;
}

syncer::ModelType NonBlockingDataTypeController::type() const {
  return model_type_;
}

void NonBlockingDataTypeController::InitializeProcessor() {
  base::WeakPtr<syncer_v2::ModelTypeService> model_type_service =
      sync_client_->GetModelTypeServiceForType(type());
  if (!model_type_service.get()) {
    LOG(WARNING) << "ModelTypeService destroyed before "
                    "ModelTypeController was started.";
    // TODO(gangwu): Add SyncError and then call start_callback with it. also
    // set an error state to |state_|.
  }

  scoped_ptr<syncer_v2::SharedModelTypeProcessor> shared_model_type_processor(
      make_scoped_ptr(new syncer_v2::SharedModelTypeProcessor(
          type(), model_type_service.get())));
  type_processor_ = shared_model_type_processor->AsWeakPtrForUI();
  model_type_service->set_change_processor(
      std::move(shared_model_type_processor));
}

}  // namespace sync_driver_v2
