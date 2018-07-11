// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/fake_data_type_controller.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/model/data_type_error_handler_impl.h"
#include "components/sync/model/sync_merge_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

FakeDataTypeController::FakeDataTypeController(ModelType type)
    : DirectoryDataTypeController(type,
                                  base::Closure(),
                                  nullptr,
                                  GROUP_PASSIVE),
      state_(NOT_RUNNING),
      model_load_delayed_(false),
      ready_for_start_(true),
      should_load_model_before_configure_(false),
      register_with_backend_call_count_(0),
      clear_metadata_call_count_(0) {}

FakeDataTypeController::~FakeDataTypeController() {}

bool FakeDataTypeController::ShouldLoadModelBeforeConfigure() const {
  return should_load_model_before_configure_;
}

// NOT_RUNNING ->MODEL_LOADED |MODEL_STARTING.
void FakeDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  model_load_callback_ = model_load_callback;
  if (state_ != NOT_RUNNING) {
    ADD_FAILURE();
    return;
  }

  if (model_load_delayed_ == false) {
    if (load_error_.IsSet())
      state_ = FAILED;
    else
      state_ = MODEL_LOADED;
    model_load_callback.Run(type(), load_error_);
  } else {
    state_ = MODEL_STARTING;
  }
}

void FakeDataTypeController::RegisterWithBackend(
    base::Callback<void(bool)> set_downloaded,
    ModelTypeConfigurer* configurer) {
  ++register_with_backend_call_count_;
}

// MODEL_LOADED -> MODEL_STARTING.
void FakeDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(CalledOnValidThread());
  last_start_callback_ = start_callback;
  state_ = ASSOCIATING;
}

// MODEL_STARTING | ASSOCIATING -> RUNNING | FAILED | NOT_RUNNING
// (depending on |result|)
void FakeDataTypeController::FinishStart(ConfigureResult result) {
  DCHECK(CalledOnValidThread());
  // We should have a callback from Start().
  if (last_start_callback_.is_null()) {
    ADD_FAILURE();
    return;
  }

  // Set |state_| first below since the callback may call state().
  SyncMergeResult local_merge_result(type());
  SyncMergeResult syncer_merge_result(type());
  if (result <= OK_FIRST_RUN) {
    state_ = RUNNING;
  } else if (result == ASSOCIATION_FAILED) {
    state_ = FAILED;
    local_merge_result.set_error(SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                                           "Association failed", type()));
  } else if (result == UNRECOVERABLE_ERROR) {
    state_ = NOT_RUNNING;
    local_merge_result.set_error(SyncError(FROM_HERE,
                                           SyncError::UNRECOVERABLE_ERROR,
                                           "Unrecoverable error", type()));
  } else if (result == NEEDS_CRYPTO) {
    state_ = NOT_RUNNING;
    local_merge_result.set_error(
        SyncError(FROM_HERE, SyncError::CRYPTO_ERROR, "Crypto error", type()));
  } else {
    NOTREACHED();
  }
  last_start_callback_.Run(result, local_merge_result, syncer_merge_result);
}

// * -> NOT_RUNNING
void FakeDataTypeController::Stop(SyncStopMetadataFate metadata_fate) {
  DCHECK(CalledOnValidThread());
  if (state() == MODEL_STARTING) {
    // Real data type controllers run the callback and specify "ABORTED" as an
    // error.  We should probably find a way to use the real code and mock out
    // unnecessary pieces.
    SimulateModelLoadFinishing();
  }

  if (metadata_fate == CLEAR_METADATA)
    ++clear_metadata_call_count_;

  state_ = NOT_RUNNING;
}

ChangeProcessor* FakeDataTypeController::GetChangeProcessor() const {
  return nullptr;
}

DataTypeController::State FakeDataTypeController::state() const {
  return state_;
}

bool FakeDataTypeController::ReadyForStart() const {
  return ready_for_start_;
}

void FakeDataTypeController::SetDelayModelLoad() {
  model_load_delayed_ = true;
}

void FakeDataTypeController::SetModelLoadError(SyncError error) {
  load_error_ = error;
}

void FakeDataTypeController::SimulateModelLoadFinishing() {
  if (load_error_.IsSet())
    state_ = FAILED;
  else
    state_ = MODEL_LOADED;
  model_load_callback_.Run(type(), load_error_);
}

void FakeDataTypeController::SetReadyForStart(bool ready) {
  ready_for_start_ = ready;
}

void FakeDataTypeController::SetShouldLoadModelBeforeConfigure(bool value) {
  should_load_model_before_configure_ = value;
}

std::unique_ptr<DataTypeErrorHandler>
FakeDataTypeController::CreateErrorHandler() {
  DCHECK(CalledOnValidThread());
  return std::make_unique<DataTypeErrorHandlerImpl>(
      base::SequencedTaskRunnerHandle::Get(), base::Closure(),
      base::Bind(model_load_callback_, type()));
}

}  // namespace syncer
