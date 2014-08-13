// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/ui_data_type_controller.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_driver/generic_change_processor_factory.h"
#include "components/sync_driver/shared_change_processor_ref.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/util/data_type_histogram.h"

namespace sync_driver {

UIDataTypeController::UIDataTypeController()
    : DataTypeController(base::MessageLoopProxy::current(),
                         base::Closure(),
                         DisableTypeCallback()),
      sync_factory_(NULL),
      state_(NOT_RUNNING),
      type_(syncer::UNSPECIFIED) {
}

UIDataTypeController::UIDataTypeController(
    scoped_refptr<base::MessageLoopProxy> ui_thread,
    const base::Closure& error_callback,
    const DisableTypeCallback& disable_callback,
    syncer::ModelType type,
    SyncApiComponentFactory* sync_factory)
    : DataTypeController(ui_thread, error_callback, disable_callback),
      sync_factory_(sync_factory),
      state_(NOT_RUNNING),
      type_(type),
      processor_factory_(new GenericChangeProcessorFactory()),
      ui_thread_(ui_thread) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(sync_factory);
  DCHECK(syncer::IsRealDataType(type_));
}

void UIDataTypeController::SetGenericChangeProcessorFactoryForTest(
      scoped_ptr<GenericChangeProcessorFactory> factory) {
  DCHECK_EQ(state_, NOT_RUNNING);
  processor_factory_ = factory.Pass();
}

UIDataTypeController::~UIDataTypeController() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
}

void UIDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(!model_load_callback.is_null());
  DCHECK(syncer::IsRealDataType(type_));
  if (state_ != NOT_RUNNING) {
    model_load_callback.Run(type(),
                            syncer::SyncError(FROM_HERE,
                                              syncer::SyncError::DATATYPE_ERROR,
                                              "Model already loaded",
                                              type()));
    return;
  }
  // Since we can't be called multiple times before Stop() is called,
  // |shared_change_processor_| must be NULL here.
  DCHECK(!shared_change_processor_.get());
  shared_change_processor_ = new SharedChangeProcessor();

  model_load_callback_ = model_load_callback;
  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early. state_ will control
    // what we perform next.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING);
    return;
  }

  state_ = MODEL_LOADED;
  model_load_callback_.Reset();
  model_load_callback.Run(type(), syncer::SyncError());
}

void UIDataTypeController::OnModelLoaded() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(!model_load_callback_.is_null());
  DCHECK_EQ(state_, MODEL_STARTING);

  state_ = MODEL_LOADED;
  ModelLoadCallback model_load_callback = model_load_callback_;
  model_load_callback_.Reset();
  model_load_callback.Run(type(), syncer::SyncError());
}

void UIDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(!start_callback.is_null());
  DCHECK_EQ(state_, MODEL_LOADED);

  start_callback_ = start_callback;
  state_ = ASSOCIATING;
  Associate();
  // It's possible StartDone(..) resulted in a Stop() call, or that association
  // failed, so we just verify that the state has moved foward.
  DCHECK_NE(state_, ASSOCIATING);
}

bool UIDataTypeController::StartModels() {
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

void UIDataTypeController::Associate() {
  DCHECK_EQ(state_, ASSOCIATING);
  syncer::SyncMergeResult local_merge_result(type());
  syncer::SyncMergeResult syncer_merge_result(type());
  base::WeakPtrFactory<syncer::SyncMergeResult> weak_ptr_factory(
      &syncer_merge_result);

  // Connect |shared_change_processor_| to the syncer and get the
  // syncer::SyncableService associated with type().
  local_service_ = shared_change_processor_->Connect(
      sync_factory_,
      processor_factory_.get(),
      user_share(),
      this,
      type(),
      weak_ptr_factory.GetWeakPtr());
  if (!local_service_.get()) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Failed to connect to syncer.",
                            type());
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  if (!shared_change_processor_->CryptoReadyIfNecessary()) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::CRYPTO_ERROR,
                            "",
                            type());
    local_merge_result.set_error(error);
    StartDone(NEEDS_CRYPTO,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  bool sync_has_nodes = false;
  if (!shared_change_processor_->SyncModelHasUserCreatedNodes(
          &sync_has_nodes)) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::UNRECOVERABLE_ERROR,
                            "Failed to load sync nodes",
                            type());
    local_merge_result.set_error(error);
    StartDone(UNRECOVERABLE_ERROR,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  syncer::SyncDataList initial_sync_data;
  syncer::SyncError error =
      shared_change_processor_->GetAllSyncDataReturnError(
          type(), &initial_sync_data);
  if (error.IsSet()) {
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  std::string datatype_context;
  if (shared_change_processor_->GetDataTypeContext(&datatype_context)) {
    local_service_->UpdateDataTypeContext(
        type(), syncer::SyncChangeProcessor::NO_REFRESH, datatype_context);
  }

  syncer_merge_result.set_num_items_before_association(
      initial_sync_data.size());
  // Passes a reference to |shared_change_processor_|.
  local_merge_result = local_service_->MergeDataAndStartSyncing(
      type(),
      initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SharedChangeProcessorRef(shared_change_processor_)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SharedChangeProcessorRef(shared_change_processor_)));
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (local_merge_result.error().IsSet()) {
    StartDone(ASSOCIATION_FAILED,
              local_merge_result,
              syncer_merge_result);
    return;
  }

  syncer_merge_result.set_num_items_after_association(
      shared_change_processor_->GetSyncCount());

  state_ = RUNNING;
  StartDone(sync_has_nodes ? OK : OK_FIRST_RUN,
            local_merge_result,
            syncer_merge_result);
}

ChangeProcessor* UIDataTypeController::GetChangeProcessor() const {
  DCHECK_EQ(state_, RUNNING);
  return shared_change_processor_->generic_change_processor();
}

void UIDataTypeController::AbortModelLoad() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  state_ = NOT_RUNNING;

  if (shared_change_processor_.get()) {
    shared_change_processor_ = NULL;
  }

  ModelLoadCallback model_load_callback = model_load_callback_;
  model_load_callback_.Reset();
  model_load_callback.Run(type(),
                          syncer::SyncError(FROM_HERE,
                                            syncer::SyncError::DATATYPE_ERROR,
                                            "Aborted",
                                            type()));
  // We don't want to continue loading models (e.g OnModelLoaded should never be
  // called after we've decided to abort).
  StopModels();
}

void UIDataTypeController::StartDone(
    ConfigureResult start_result,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK(ui_thread_->BelongsToCurrentThread());

  if (!IsSuccessfulResult(start_result)) {
    StopModels();
    if (start_result == ASSOCIATION_FAILED) {
      state_ = DISABLED;
    } else {
      state_ = NOT_RUNNING;
    }
    RecordStartFailure(start_result);

    if (shared_change_processor_.get()) {
      shared_change_processor_->Disconnect();
      shared_change_processor_ = NULL;
    }
  }

  start_callback_.Run(start_result, local_merge_result, syncer_merge_result);
}

void UIDataTypeController::Stop() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(syncer::IsRealDataType(type_));

  State prev_state = state_;
  state_ = STOPPING;

  if (shared_change_processor_.get()) {
    shared_change_processor_->Disconnect();
    shared_change_processor_ = NULL;
  }

  // If Stop() is called while Start() is waiting for the datatype model to
  // load, abort the start.
  if (prev_state == MODEL_STARTING) {
    AbortModelLoad();
    // We can just return here since we haven't performed association if we're
    // still in MODEL_STARTING.
    return;
  }

  StopModels();

  if (local_service_.get()) {
    local_service_->StopSyncing(type());
  }

  state_ = NOT_RUNNING;
}

syncer::ModelType UIDataTypeController::type() const {
  DCHECK(syncer::IsRealDataType(type_));
  return type_;
}

void UIDataTypeController::StopModels() {
  // Do nothing by default.
}

syncer::ModelSafeGroup UIDataTypeController::model_safe_group() const {
  DCHECK(syncer::IsRealDataType(type_));
  return syncer::GROUP_UI;
}

std::string UIDataTypeController::name() const {
  // For logging only.
  return syncer::ModelTypeToString(type());
}

DataTypeController::State UIDataTypeController::state() const {
  return state_;
}

void UIDataTypeController::OnSingleDatatypeUnrecoverableError(
    const tracked_objects::Location& from_here, const std::string& message) {
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
  // TODO(tim): We double-upload some errors.  See bug 383480.
  if (!error_callback_.is_null())
    error_callback_.Run();
  if (!start_callback_.is_null()) {
    syncer::SyncError error(
        from_here, syncer::SyncError::DATATYPE_ERROR, message, type());
    syncer::SyncMergeResult local_merge_result(type());
    local_merge_result.set_error(error);
    start_callback_.Run(RUNTIME_ERROR,
                        local_merge_result,
                        syncer::SyncMergeResult(type()));
  }
}

void UIDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void UIDataTypeController::RecordStartFailure(ConfigureResult result) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_ENUMERATION("Sync." type_str "StartFailure", result, \
                              MAX_START_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

}  // namespace sync_driver
