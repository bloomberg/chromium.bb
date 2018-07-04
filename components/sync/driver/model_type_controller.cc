// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/signin/core/browser/account_info.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_error_handler_impl.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

using DelegateProvider = ModelTypeController::DelegateProvider;
using ModelTask = ModelTypeController::ModelTask;

namespace {

void OnSyncStartingHelperOnModelThread(
    const DataTypeActivationRequest& request,
    ModelTypeControllerDelegate::StartCallback callback_bound_to_ui_thread,
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  delegate->OnSyncStarting(request, std::move(callback_bound_to_ui_thread));
}

void GetAllNodesForDebuggingHelperOnModelThread(
    const DataTypeController::AllNodesCallback& callback_bound_to_ui_thread,
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  delegate->GetAllNodesForDebugging(callback_bound_to_ui_thread);
}

void GetStatusCountersForDebuggingHelperOnModelThread(
    const DataTypeController::StatusCountersCallback&
        callback_bound_to_ui_thread,
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  delegate->GetStatusCountersForDebugging(callback_bound_to_ui_thread);
}

void RecordMemoryUsageHistogramHelperOnModelThread(
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  delegate->RecordMemoryUsageHistogram();
}

void StopSyncHelperOnModelThread(
    SyncStopMetadataFate metadata_fate,
    base::WeakPtr<ModelTypeControllerDelegate> delegate) {
  delegate->OnSyncStopping(metadata_fate);
}

void ReportError(ModelType model_type,
                 scoped_refptr<base::SequencedTaskRunner> ui_thread,
                 const ModelErrorHandler& error_handler,
                 const ModelError& error) {
  // TODO(wychen): enum uma should be strongly typed. crbug.com/661401
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(model_type),
                            static_cast<int>(MODEL_TYPE_COUNT));
  ui_thread->PostTask(error.location(), base::Bind(error_handler, error));
}

// This function allows us to return a Callback using Bind that returns the
// given |arg|. This function itself does nothing.
base::WeakPtr<ModelTypeControllerDelegate> ReturnCapturedDelegate(
    base::WeakPtr<ModelTypeControllerDelegate> arg) {
  return arg;
}

void RunModelTask(DelegateProvider delegate_provider, ModelTask task) {
  base::WeakPtr<ModelTypeControllerDelegate> delegate =
      std::move(delegate_provider).Run();
  if (delegate.get())
    std::move(task).Run(delegate);
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
    SyncClient* sync_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& model_thread)
    : DataTypeController(type),
      sync_client_(sync_client),
      model_thread_(model_thread),
      state_(NOT_RUNNING) {}

ModelTypeController::~ModelTypeController() {}

bool ModelTypeController::ShouldLoadModelBeforeConfigure() const {
  // USS datatypes require loading models because model controls storage where
  // data type context and progress marker are persisted.
  return true;
}

void ModelTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!model_load_callback.is_null());
  DCHECK_EQ(NOT_RUNNING, state_);

  model_load_callback_ = model_load_callback;

  DVLOG(1) << "Sync starting for " << ModelTypeToString(type());
  state_ = MODEL_STARTING;

  // Callback that posts back to the UI thread.
  ModelTypeControllerDelegate::StartCallback callback_bound_to_ui_thread =
      BindToCurrentSequence(base::BindOnce(
          &ModelTypeController::OnProcessorStarted, base::AsWeakPtr(this)));

  DataTypeActivationRequest request;
  request.error_handler = base::BindRepeating(
      &ReportError, type(), base::SequencedTaskRunnerHandle::Get(),
      base::BindRepeating(&ModelTypeController::ReportModelError,
                          base::AsWeakPtr(this), SyncError::DATATYPE_ERROR));
  request.authenticated_account_id =
      sync_client_->GetSyncService()->GetAuthenticatedAccountInfo().account_id;
  request.cache_guid = sync_client_->GetSyncService()
                           ->GetLocalDeviceInfoProvider()
                           ->GetLocalSyncCacheGUID();

  DCHECK(!request.authenticated_account_id.empty());
  DCHECK(!request.cache_guid.empty());

  // Start the type processor on the model thread.
  PostModelTask(
      FROM_HERE,
      base::BindOnce(&OnSyncStartingHelperOnModelThread, std::move(request),
                     std::move(callback_bound_to_ui_thread)));
}

void ModelTypeController::BeforeLoadModels(ModelTypeConfigurer* configurer) {}

void ModelTypeController::LoadModelsDone(ConfigureResult result,
                                         const SyncError& error) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(NOT_RUNNING, state_);

  if (state_ == STOPPING) {
    DCHECK(!model_stop_callbacks_.empty());
    // This reply to OnSyncStarting() has arrived after the type has been
    // requested to stop.
    DVLOG(1) << "Sync start completion received late for "
             << ModelTypeToString(type()) << ", it has been stopped meanwhile";
    RecordStartFailure(ABORTED);
    PostModelTask(FROM_HERE, base::BindOnce(&StopSyncHelperOnModelThread,
                                            model_stop_metadata_fate_));
    state_ = NOT_RUNNING;

    // We make a copy in case running the callbacks has side effects and
    // modifies the vector, although we don't expect that in practice.
    std::vector<StopCallback> model_stop_callbacks =
        std::move(model_stop_callbacks_);
    DCHECK(model_stop_callbacks_.empty());
    for (StopCallback& stop_callback : model_stop_callbacks) {
      std::move(stop_callback).Run();
    }
    return;
  }

  if (IsSuccessfulResult(result)) {
    DCHECK_EQ(MODEL_STARTING, state_);
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
  DCHECK_EQ(MODEL_LOADED, state_);
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
  DCHECK_EQ(MODEL_LOADED, state_);

  state_ = RUNNING;
  DVLOG(1) << "Sync running for " << ModelTypeToString(type());

  // There is no association, just call back promptly.
  SyncMergeResult merge_result(type());
  start_callback.Run(OK, merge_result, merge_result);
}

void ModelTypeController::ActivateDataType(ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  DCHECK(configurer);
  DCHECK_EQ(RUNNING, state_);
  // In contrast with directory datatypes, non-blocking data types should be
  // activated in RegisterWithBackend. activation_response_ should be
  // passed to backend before call to ActivateDataType.
  DCHECK(!activation_response_);
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
      DCHECK(!model_stop_callbacks_.empty());
      model_stop_metadata_fate_ =
          TakeStrictestMetadataFate(model_stop_metadata_fate_, metadata_fate);
      model_stop_callbacks_.push_back(std::move(callback));
      break;

    case MODEL_STARTING:
      DCHECK(!model_load_callback_.is_null());
      DCHECK(model_stop_callbacks_.empty());
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
      PostModelTask(FROM_HERE, base::BindOnce(&StopSyncHelperOnModelThread,
                                              metadata_fate));
      state_ = NOT_RUNNING;
      std::move(callback).Run();
      break;
  }
}

DataTypeController::State ModelTypeController::state() const {
  return state_;
}

void ModelTypeController::GetAllNodes(const AllNodesCallback& callback) {
  PostModelTask(FROM_HERE,
                base::BindOnce(&GetAllNodesForDebuggingHelperOnModelThread,
                               BindToCurrentSequence(callback)));
}

void ModelTypeController::GetStatusCounters(
    const StatusCountersCallback& callback) {
  PostModelTask(
      FROM_HERE,
      base::BindOnce(&GetStatusCountersForDebuggingHelperOnModelThread,
                     BindToCurrentSequence(callback)));
}

void ModelTypeController::RecordMemoryUsageHistogram() {
  PostModelTask(FROM_HERE,
                base::BindOnce(&RecordMemoryUsageHistogramHelperOnModelThread));
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

DelegateProvider ModelTypeController::GetDelegateProvider() {
  // Get the delegate eagerly, and capture the weak pointer.
  base::WeakPtr<ModelTypeControllerDelegate> delegate =
      sync_client_->GetControllerDelegateForModelType(type());
  return base::Bind(&ReturnCapturedDelegate, delegate);
}

void ModelTypeController::PostModelTask(const base::Location& location,
                                        ModelTask task) {
  DCHECK(model_thread_);
  model_thread_->PostTask(
      location,
      base::BindOnce(&RunModelTask, GetDelegateProvider(), std::move(task)));
}

}  // namespace syncer
