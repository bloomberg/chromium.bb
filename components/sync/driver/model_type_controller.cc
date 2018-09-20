// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/signin/core/browser/account_info.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_error_handler_impl.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {
namespace {

void ReportError(ModelType model_type,
                 scoped_refptr<base::SequencedTaskRunner> ui_thread,
                 const ModelErrorHandler& error_handler,
                 const ModelError& error) {
  // TODO(wychen): enum uma should be strongly typed. crbug.com/661401
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(model_type),
                            static_cast<int>(MODEL_TYPE_COUNT));
  ui_thread->PostTask(error.location(), base::BindOnce(error_handler, error));
}

// Takes the strictest policy for clearing sync metadata.
SyncStopMetadataFate TakeStrictestMetadataFate(SyncStopMetadataFate fate1,
                                               SyncStopMetadataFate fate2) {
  switch (fate1) {
    case CLEAR_METADATA:
      return CLEAR_METADATA;
    case KEEP_METADATA:
      return fate2;
  }
  NOTREACHED();
  return KEEP_METADATA;
}

}  // namespace

ModelTypeController::ModelTypeController(
    ModelType type,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_on_disk)
    : DataTypeController(type), state_(NOT_RUNNING) {
  delegate_map_.emplace(ConfigureContext::STORAGE_ON_DISK,
                        std::move(delegate_on_disk));
}

ModelTypeController::ModelTypeController(
    ModelType type,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_on_disk,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_in_memory)
    : ModelTypeController(type, std::move(delegate_on_disk)) {
  delegate_map_.emplace(ConfigureContext::STORAGE_IN_MEMORY,
                        std::move(delegate_in_memory));
}

ModelTypeController::~ModelTypeController() {}

bool ModelTypeController::ShouldLoadModelBeforeConfigure() const {
  // USS datatypes require loading models because model controls storage where
  // data type context and progress marker are persisted.
  return true;
}

void ModelTypeController::LoadModels(
    const ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  CHECK(!model_load_callback.is_null());
  CHECK_EQ(NOT_RUNNING, state_);

  auto it = delegate_map_.find(configure_context.storage_option);
  CHECK(it != delegate_map_.end());
  delegate_ = it->second.get();
  CHECK(delegate_);

  DVLOG(1) << "Sync starting for " << ModelTypeToString(type());
  state_ = MODEL_STARTING;
  model_load_callback_ = model_load_callback;

  DataTypeActivationRequest request;
  request.error_handler = base::BindRepeating(
      &ReportError, type(), base::SequencedTaskRunnerHandle::Get(),
      base::BindRepeating(&ModelTypeController::ReportModelError,
                          base::AsWeakPtr(this), SyncError::DATATYPE_ERROR));
  request.authenticated_account_id = configure_context.authenticated_account_id;
  request.cache_guid = configure_context.cache_guid;

  // Note that |request.authenticated_account_id| may be empty for local sync.
  DCHECK(!request.cache_guid.empty());

  // Ask the delegate to actually start the datatype.
  delegate_->OnSyncStarting(
      request, base::BindOnce(&ModelTypeController::OnProcessorStarted,
                              base::AsWeakPtr(this)));
}

void ModelTypeController::BeforeLoadModels(ModelTypeConfigurer* configurer) {}

void ModelTypeController::LoadModelsDone(ConfigureResult result,
                                         const SyncError& error) {
  DCHECK(CalledOnValidThread());
  CHECK_NE(NOT_RUNNING, state_);

  if (state_ == STOPPING) {
    DCHECK(!model_stop_callbacks_.empty());
    // This reply to OnSyncStarting() has arrived after the type has been
    // requested to stop.
    DVLOG(1) << "Sync start completion received late for "
             << ModelTypeToString(type()) << ", it has been stopped meanwhile";
    RecordStartFailure(ABORTED);
    state_ = NOT_RUNNING;

    // We make a copy in case running the callbacks has side effects and
    // modifies the vector, although we don't expect that in practice.
    std::vector<StopCallback> model_stop_callbacks =
        std::move(model_stop_callbacks_);
    DCHECK(model_stop_callbacks_.empty());

    delegate_->OnSyncStopping(model_stop_metadata_fate_);
    delegate_ = nullptr;

    for (StopCallback& stop_callback : model_stop_callbacks) {
      std::move(stop_callback).Run();
    }
    return;
  }

  if (IsSuccessfulResult(result)) {
    CHECK_EQ(MODEL_STARTING, state_);
    state_ = MODEL_LOADED;
    DVLOG(1) << "Sync start completed for " << ModelTypeToString(type());
  } else {
    RecordStartFailure(result);
    state_ = FAILED;
  }

  if (!model_load_callback_.is_null()) {
    model_load_callback_.Run(type(), error);
  }
}

void ModelTypeController::OnProcessorStarted(
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  DCHECK(CalledOnValidThread());
  // Hold on to the activation context until ActivateDataType is called.
  if (state_ == MODEL_STARTING) {
    activation_response_ = std::move(activation_response);
  }
  LoadModelsDone(OK, SyncError());
}

void ModelTypeController::RegisterWithBackend(
    base::Callback<void(bool)> set_downloaded,
    ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  if (activated_)
    return;
  DCHECK(configurer);
  DCHECK(activation_response_);
  CHECK_EQ(MODEL_LOADED, state_);
  // Inform the DataTypeManager whether our initial download is complete.
  set_downloaded.Run(
      activation_response_->model_type_state.initial_sync_done());
  // Pass activation context to ModelTypeRegistry, where ModelTypeWorker gets
  // created and connected with ModelTypeProcessor.
  configurer->ActivateNonBlockingDataType(type(),
                                          std::move(activation_response_));
  activated_ = true;
}

void ModelTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!start_callback.is_null());
  CHECK_EQ(MODEL_LOADED, state_);

  state_ = RUNNING;
  DVLOG(1) << "Sync running for " << ModelTypeToString(type());

  // There is no association, just call back promptly.
  SyncMergeResult merge_result(type());
  start_callback.Run(OK, merge_result, merge_result);
}

void ModelTypeController::ActivateDataType(ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  DCHECK(configurer);
  CHECK_EQ(RUNNING, state_);
  // In contrast with directory datatypes, non-blocking data types should be
  // activated in RegisterWithBackend. activation_response_ should be
  // passed to backend before call to ActivateDataType.
  CHECK(!activation_response_);
}

void ModelTypeController::DeactivateDataType(ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  DCHECK(configurer);
  if (activated_) {
    configurer->DeactivateNonBlockingDataType(type());
    activated_ = false;
  }
}

void ModelTypeController::Stop(SyncStopMetadataFate metadata_fate,
                               StopCallback callback) {
  DCHECK(CalledOnValidThread());

  switch (state()) {
    case ASSOCIATING:
      // We don't really use this state in this class.
      NOTREACHED();
      break;

    case NOT_RUNNING:
    case FAILED:
      // Nothing to stop. |metadata_fate| might require CLEAR_METADATA,
      // which could lead to leaking sync metadata, but it doesn't seem a
      // realistic scenario (disable sync during shutdown?).
      return;

    case STOPPING:
      CHECK(!model_stop_callbacks_.empty());
      model_stop_metadata_fate_ =
          TakeStrictestMetadataFate(model_stop_metadata_fate_, metadata_fate);
      model_stop_callbacks_.push_back(std::move(callback));
      break;

    case MODEL_STARTING:
      CHECK(!model_load_callback_.is_null());
      CHECK(model_stop_callbacks_.empty());
      DLOG(WARNING) << "Deferring stop for " << ModelTypeToString(type())
                    << " because it's still starting";
      model_stop_metadata_fate_ = metadata_fate;
      model_stop_callbacks_.push_back(std::move(callback));
      // The actual stop will be executed in LoadModelsDone(), when the starting
      // process is finished.
      state_ = STOPPING;
      break;

    case MODEL_LOADED:
    case RUNNING:
      DVLOG(1) << "Stopping sync for " << ModelTypeToString(type());
      state_ = NOT_RUNNING;
      delegate_->OnSyncStopping(metadata_fate);
      delegate_ = nullptr;
      std::move(callback).Run();
      break;
  }
}

DataTypeController::State ModelTypeController::state() const {
  return state_;
}

void ModelTypeController::GetAllNodes(const AllNodesCallback& callback) {
  DCHECK(delegate_);
  delegate_->GetAllNodesForDebugging(callback);
}

void ModelTypeController::GetStatusCounters(
    const StatusCountersCallback& callback) {
  DCHECK(delegate_);
  delegate_->GetStatusCountersForDebugging(callback);
}

void ModelTypeController::RecordMemoryUsageAndCountsHistograms() {
  DCHECK(delegate_);
  delegate_->RecordMemoryUsageAndCountsHistograms();
}

void ModelTypeController::ReportModelError(SyncError::ErrorType error_type,
                                           const ModelError& error) {
  DCHECK(CalledOnValidThread());
  LoadModelsDone(UNRECOVERABLE_ERROR, SyncError(error.location(), error_type,
                                                error.message(), type()));
}

void ModelTypeController::RecordStartFailure(ConfigureResult result) const {
  DCHECK(CalledOnValidThread());
  // TODO(wychen): enum uma should be strongly typed. crbug.com/661401
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures",
                            ModelTypeToHistogramInt(type()),
                            static_cast<int>(MODEL_TYPE_COUNT));
#define PER_DATA_TYPE_MACRO(type_str)                                    \
  UMA_HISTOGRAM_ENUMERATION("Sync." type_str "ConfigureFailure", result, \
                            MAX_CONFIGURE_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

}  // namespace syncer
