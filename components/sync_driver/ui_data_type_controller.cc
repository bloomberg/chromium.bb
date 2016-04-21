// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/ui_data_type_controller.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/sync_driver/generic_change_processor_factory.h"
#include "components/sync_driver/shared_change_processor_ref.h"
#include "components/sync_driver/sync_client.h"
#include "components/sync_driver/sync_service.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_merge_result.h"
#include "sync/api/syncable_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/util/data_type_histogram.h"

namespace sync_driver {

UIDataTypeController::UIDataTypeController()
    : DirectoryDataTypeController(base::ThreadTaskRunnerHandle::Get(),
                                  base::Closure()),
      sync_client_(NULL),
      state_(NOT_RUNNING),
      type_(syncer::UNSPECIFIED) {}

UIDataTypeController::UIDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    syncer::ModelType type,
    SyncClient* sync_client)
    : DirectoryDataTypeController(ui_thread, error_callback),
      sync_client_(sync_client),
      state_(NOT_RUNNING),
      type_(type),
      processor_factory_(new GenericChangeProcessorFactory()),
      ui_thread_(ui_thread) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(syncer::IsRealDataType(type_));
}

void UIDataTypeController::SetGenericChangeProcessorFactoryForTest(
    std::unique_ptr<GenericChangeProcessorFactory> factory) {
  DCHECK_EQ(state_, NOT_RUNNING);
  processor_factory_ = std::move(factory);
}

UIDataTypeController::~UIDataTypeController() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
}

void UIDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(syncer::IsRealDataType(type_));
  model_load_callback_ = model_load_callback;
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

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early. state_ will control
    // what we perform next.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING);
    return;
  }

  state_ = MODEL_LOADED;
  model_load_callback_.Run(type(), syncer::SyncError());
}

void UIDataTypeController::OnModelLoaded() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK_EQ(state_, MODEL_STARTING);

  state_ = MODEL_LOADED;
  model_load_callback_.Run(type(), syncer::SyncError());
}

void UIDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(!start_callback.is_null());
  DCHECK_EQ(state_, MODEL_LOADED);

  start_callback_ = start_callback;
  state_ = ASSOCIATING;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&UIDataTypeController::Associate, this));
}

bool UIDataTypeController::StartModels() {
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

void UIDataTypeController::Associate() {
  if (state_ != ASSOCIATING) {
    // Stop() must have been called while Associate() task have been waiting.
    DCHECK_EQ(state_, NOT_RUNNING);
    return;
  }

  syncer::SyncMergeResult local_merge_result(type());
  syncer::SyncMergeResult syncer_merge_result(type());
  base::WeakPtrFactory<syncer::SyncMergeResult> weak_ptr_factory(
      &syncer_merge_result);

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/471403 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "471403 UIDataTypeController::Associate1"));

  // Connect |shared_change_processor_| to the syncer and get the
  // syncer::SyncableService associated with type().
  DCHECK(sync_client_->GetSyncService());
  local_service_ = shared_change_processor_->Connect(
      sync_client_, processor_factory_.get(),
      sync_client_->GetSyncService()->GetUserShare(), this, type(),
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

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/471403 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "471403 UIDataTypeController::Associate2"));
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

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/471403 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "471403 UIDataTypeController::Associate3"));
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

  // Scope for |initial_sync_data| which might be expensive, so we don't want
  // to keep it in memory longer than necessary.
  {
    syncer::SyncDataList initial_sync_data;

    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/471403
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile4(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "471403 UIDataTypeController::Associate4"));
    base::TimeTicks start_time = base::TimeTicks::Now();
    syncer::SyncError error =
        shared_change_processor_->GetAllSyncDataReturnError(type(),
                                                            &initial_sync_data);
    if (error.IsSet()) {
      local_merge_result.set_error(error);
      StartDone(ASSOCIATION_FAILED, local_merge_result, syncer_merge_result);
      return;
    }

    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/471403
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile5(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "471403 UIDataTypeController::Associate5"));
    std::string datatype_context;
    if (shared_change_processor_->GetDataTypeContext(&datatype_context)) {
      local_service_->UpdateDataTypeContext(
          type(), syncer::SyncChangeProcessor::NO_REFRESH, datatype_context);
    }

    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/471403
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile6(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "471403 UIDataTypeController::Associate6"));
    syncer_merge_result.set_num_items_before_association(
        initial_sync_data.size());
    // Passes a reference to |shared_change_processor_|.
    local_merge_result = local_service_->MergeDataAndStartSyncing(
        type(), initial_sync_data,
        std::unique_ptr<syncer::SyncChangeProcessor>(
            new SharedChangeProcessorRef(shared_change_processor_)),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new SharedChangeProcessorRef(shared_change_processor_)));
    RecordAssociationTime(base::TimeTicks::Now() - start_time);
    if (local_merge_result.error().IsSet()) {
      StartDone(ASSOCIATION_FAILED, local_merge_result, syncer_merge_result);
      return;
    }
  }

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/471403 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile7(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "471403 UIDataTypeController::Associate7"));
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

  // We don't want to continue loading models (e.g OnModelLoaded should never be
  // called after we've decided to abort).
  StopModels();
}

void UIDataTypeController::StartDone(
    ConfigureResult start_result,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK(ui_thread_->BelongsToCurrentThread());

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/471403 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "471403 UIDataTypeController::StartDone"));

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

  if (state_ == NOT_RUNNING)
    return;

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

void UIDataTypeController::OnSingleDataTypeUnrecoverableError(
    const syncer::SyncError& error) {
  DCHECK_EQ(type(), error.model_type());
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
  // TODO(tim): We double-upload some errors.  See bug 383480.
  if (!error_callback_.is_null())
    error_callback_.Run();
  if (!model_load_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(model_load_callback_, type(), error));
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
#define PER_DATA_TYPE_MACRO(type_str)                                    \
  UMA_HISTOGRAM_ENUMERATION("Sync." type_str "ConfigureFailure", result, \
                            MAX_CONFIGURE_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

}  // namespace sync_driver
