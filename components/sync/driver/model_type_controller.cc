// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/model/data_type_error_handler_impl.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_debug_info.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

using BridgeProvider = ModelTypeController::BridgeProvider;
using BridgeTask = ModelTypeController::BridgeTask;

namespace {

void OnSyncStartingHelper(
    const ModelErrorHandler& error_handler,
    const ModelTypeChangeProcessor::StartCallback& callback,
    ModelTypeSyncBridge* bridge) {
  bridge->OnSyncStarting(std::move(error_handler), std::move(callback));
}

void ReportError(ModelType model_type,
                 scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
                 const ModelErrorHandler& error_handler,
                 const ModelError& error) {
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(model_type),
                            MODEL_TYPE_COUNT);
  ui_thread->PostTask(error.location(), base::Bind(error_handler, error));
}

// This function allows us to return a Callback using Bind that returns the
// given |arg|. This function itself does nothing.
base::WeakPtr<ModelTypeSyncBridge> ReturnCapturedBridge(
    base::WeakPtr<ModelTypeSyncBridge> arg) {
  return arg;
}

void RunBridgeTask(const BridgeProvider& bridge_provider,
                   const BridgeTask& task) {
  if (base::WeakPtr<ModelTypeSyncBridge> bridge = bridge_provider.Run()) {
    task.Run(bridge.get());
  }
}

}  // namespace

ModelTypeController::ModelTypeController(
    ModelType type,
    SyncClient* sync_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& model_thread)
    : DataTypeController(type),
      sync_client_(sync_client),
      model_thread_(model_thread),
      sync_prefs_(sync_client->GetPrefService()),
      state_(NOT_RUNNING) {
  DCHECK(model_thread_);
}

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
  model_load_callback_ = model_load_callback;

  if (state() != NOT_RUNNING) {
    LoadModelsDone(RUNTIME_ERROR,
                   SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                             "Model already running", type()));
    return;
  }

  state_ = MODEL_STARTING;

  // Callback that posts back to the UI thread.
  ModelTypeChangeProcessor::StartCallback callback =
      BindToCurrentThread(base::Bind(&ModelTypeController::OnProcessorStarted,
                                     base::AsWeakPtr(this)));

  ModelErrorHandler error_handler = base::BindRepeating(
      &ReportError, type(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&ModelTypeController::ReportModelError,
                 base::AsWeakPtr(this)));

  // Start the type processor on the model thread.
  PostBridgeTask(FROM_HERE,
                 base::Bind(&OnSyncStartingHelper, std::move(error_handler),
                            std::move(callback)));
}

void ModelTypeController::BeforeLoadModels(ModelTypeConfigurer* configurer) {}

void ModelTypeController::LoadModelsDone(ConfigureResult result,
                                         const SyncError& error) {
  DCHECK(CalledOnValidThread());

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

void ModelTypeController::OnProcessorStarted(
    std::unique_ptr<ActivationContext> activation_context) {
  DCHECK(CalledOnValidThread());
  // Hold on to the activation context until ActivateDataType is called.
  if (state_ == MODEL_STARTING) {
    activation_context_ = std::move(activation_context);
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
  DCHECK(activation_context_);
  DCHECK_EQ(MODEL_LOADED, state_);
  // Inform the DataTypeManager whether our initial download is complete.
  set_downloaded.Run(activation_context_->model_type_state.initial_sync_done());
  // Pass activation context to ModelTypeRegistry, where ModelTypeWorker gets
  // created and connected with ModelTypeProcessor.
  configurer->ActivateNonBlockingDataType(type(),
                                          std::move(activation_context_));
  activated_ = true;
}

void ModelTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!start_callback.is_null());
  DCHECK_EQ(MODEL_LOADED, state_);

  state_ = RUNNING;

  // There is no association, just call back promptly.
  SyncMergeResult merge_result(type());
  start_callback.Run(OK, merge_result, merge_result);
}

void ModelTypeController::ActivateDataType(ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  DCHECK(configurer);
  DCHECK_EQ(RUNNING, state_);
  // In contrast with directory datatypes, non-blocking data types should be
  // activated in RegisterWithBackend. activation_context_ should be passed
  // to backend before call to ActivateDataType.
  DCHECK(!activation_context_);
}

void ModelTypeController::DeactivateDataType(ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  DCHECK(configurer);
  if (activated_) {
    configurer->DeactivateNonBlockingDataType(type());
    activated_ = false;
  }
}

void ModelTypeController::Stop() {
  DCHECK(CalledOnValidThread());

  if (state() == NOT_RUNNING)
    return;

  // Check preferences if datatype is not in preferred datatypes. Only call
  // DisableSync if the bridge is ready to handle it (controller is in loaded
  // state).
  ModelTypeSet preferred_types =
      sync_prefs_.GetPreferredDataTypes(ModelTypeSet(type()));
  if ((state() == MODEL_LOADED || state() == RUNNING) &&
      (!sync_prefs_.IsFirstSetupComplete() || !preferred_types.Has(type()))) {
    PostBridgeTask(FROM_HERE, base::Bind(&ModelTypeSyncBridge::DisableSync));
  }

  state_ = NOT_RUNNING;
}

std::string ModelTypeController::name() const {
  // For logging only.
  return ModelTypeToString(type());
}

DataTypeController::State ModelTypeController::state() const {
  return state_;
}

void ModelTypeController::GetAllNodes(const AllNodesCallback& callback) {
  PostBridgeTask(FROM_HERE, base::Bind(&ModelTypeDebugInfo::GetAllNodes,
                                       BindToCurrentThread(callback)));
}

void ModelTypeController::GetStatusCounters(
    const StatusCountersCallback& callback) {
  PostBridgeTask(FROM_HERE,
                 base::Bind(&ModelTypeDebugInfo::GetStatusCounters, callback));
}

void ModelTypeController::ReportModelError(const ModelError& error) {
  DCHECK(CalledOnValidThread());
  LoadModelsDone(UNRECOVERABLE_ERROR,
                 SyncError(error.location(), SyncError::DATATYPE_ERROR,
                           error.message(), type()));
}

void ModelTypeController::RecordStartFailure(ConfigureResult result) const {
  DCHECK(CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures",
                            ModelTypeToHistogramInt(type()), MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str)                                    \
  UMA_HISTOGRAM_ENUMERATION("Sync." type_str "ConfigureFailure", result, \
                            MAX_CONFIGURE_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

BridgeProvider ModelTypeController::GetBridgeProvider() {
  // Get the bridge eagerly, and capture the weak pointer.
  base::WeakPtr<ModelTypeSyncBridge> bridge =
      sync_client_->GetSyncBridgeForModelType(type());
  return base::Bind(&ReturnCapturedBridge, bridge);
}

void ModelTypeController::PostBridgeTask(
    const tracked_objects::Location& location,
    const BridgeTask& task) {
  model_thread_->PostTask(
      location, base::Bind(&RunBridgeTask, GetBridgeProvider(), task));
}
}  // namespace syncer
