// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/non_blocking_data_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/api/model_type_change_processor.h"
#include "components/sync/api/model_type_service.h"
#include "components/sync/api/sync_error.h"
#include "components/sync/api/sync_merge_result.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/core/activation_context.h"
#include "components/sync/driver/backend_data_type_configurer.h"
#include "components/sync/driver/sync_client.h"

namespace sync_driver_v2 {

NonBlockingDataTypeController::NonBlockingDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    syncer::ModelType model_type,
    sync_driver::SyncClient* sync_client)
    : sync_driver::DataTypeController(ui_thread, error_callback),
      model_type_(model_type),
      sync_client_(sync_client),
      sync_prefs_(sync_client->GetPrefService()),
      state_(NOT_RUNNING) {
  DCHECK(BelongsToUIThread());
}

NonBlockingDataTypeController::~NonBlockingDataTypeController() {}

bool NonBlockingDataTypeController::ShouldLoadModelBeforeConfigure() const {
  // USS datatypes require loading models because model controls storage where
  // data type context and progress marker are persisted.
  return true;
}

void NonBlockingDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(BelongsToUIThread());
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

  // Callback that posts back to the UI thread.
  syncer_v2::ModelTypeChangeProcessor::StartCallback callback =
      syncer::BindToTaskRunner(
          ui_thread(),
          base::Bind(&NonBlockingDataTypeController::OnProcessorStarted, this));

  // Start the type processor on the model thread.
  if (!RunOnModelThread(
          FROM_HERE,
          base::Bind(&syncer_v2::ModelTypeService::OnSyncStarting,
                     sync_client_->GetModelTypeServiceForType(type()),
                     base::Unretained(this), callback))) {
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
    std::unique_ptr<syncer_v2::ActivationContext> activation_context) {
  DCHECK(BelongsToUIThread());
  // Hold on to the activation context until ActivateDataType is called.
  if (state_ == MODEL_STARTING) {
    activation_context_ = std::move(activation_context);
  }
  // TODO(stanisc): Figure out if UNRECOVERABLE_ERROR is OK in this case.
  ConfigureResult result = error.IsSet() ? UNRECOVERABLE_ERROR : OK;
  LoadModelsDone(result, error);
}

void NonBlockingDataTypeController::RegisterWithBackend(
    sync_driver::BackendDataTypeConfigurer* configurer) {
  DCHECK(BelongsToUIThread());
  DCHECK(configurer);
  DCHECK(activation_context_);
  DCHECK_EQ(MODEL_LOADED, state_);
  configurer->ActivateNonBlockingDataType(type(),
                                          std::move(activation_context_));
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
  DCHECK_EQ(RUNNING, state_);
  // In contrast with directory datatypes, non-blocking data types should be
  // activated in RegisterWithBackend. activation_context_ should be passed
  // to backend before call to ActivateDataType.
  DCHECK(!activation_context_);
}

void NonBlockingDataTypeController::DeactivateDataType(
    sync_driver::BackendDataTypeConfigurer* configurer) {
  DCHECK(BelongsToUIThread());
  DCHECK(configurer);
  configurer->DeactivateNonBlockingDataType(type());
}

void NonBlockingDataTypeController::Stop() {
  DCHECK(BelongsToUIThread());

  if (state() == NOT_RUNNING)
    return;

  // Check preferences if datatype is not in preferred datatypes. Only call
  // DisableSync if service is ready to handle it (controller is in loaded
  // state).
  syncer::ModelTypeSet preferred_types =
      sync_prefs_.GetPreferredDataTypes(syncer::ModelTypeSet(type()));
  if ((state() == MODEL_LOADED || state() == RUNNING) &&
      (!sync_prefs_.IsFirstSetupComplete() || !preferred_types.Has(type()))) {
    RunOnModelThread(
        FROM_HERE,
        base::Bind(&syncer_v2::ModelTypeService::DisableSync,
                   sync_client_->GetModelTypeServiceForType(type())));
  }

  state_ = NOT_RUNNING;
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

syncer::ModelType NonBlockingDataTypeController::type() const {
  return model_type_;
}

}  // namespace sync_driver_v2
