// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/fake_data_type_controller.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::ModelType;

namespace sync_driver {

FakeDataTypeController::FakeDataTypeController(ModelType type)
      : DataTypeController(base::MessageLoopProxy::current(), base::Closure()),
        state_(NOT_RUNNING),
        model_load_delayed_(false),
        type_(type),
        ready_for_start_(true) {}

FakeDataTypeController::~FakeDataTypeController() {
}

// NOT_RUNNING ->MODEL_LOADED |MODEL_STARTING.
void FakeDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  model_load_callback_ = model_load_callback;
  if (state_ != NOT_RUNNING) {
    ADD_FAILURE();
    return;
  }

  if (model_load_delayed_ == false) {
    if (load_error_.IsSet())
      state_ = DISABLED;
    else
      state_ = MODEL_LOADED;
    model_load_callback.Run(type(), load_error_);
  } else {
    state_ = MODEL_STARTING;
  }
}

void FakeDataTypeController::OnModelLoaded() {
  NOTREACHED();
}

// MODEL_LOADED -> MODEL_STARTING.
void FakeDataTypeController::StartAssociating(
   const StartCallback& start_callback) {
  last_start_callback_ = start_callback;
  state_ = ASSOCIATING;
}

// MODEL_STARTING | ASSOCIATING -> RUNNING | DISABLED | NOT_RUNNING
// (depending on |result|)
void FakeDataTypeController::FinishStart(ConfigureResult result) {
  // We should have a callback from Start().
  if (last_start_callback_.is_null()) {
    ADD_FAILURE();
    return;
  }

  // Set |state_| first below since the callback may call state().
  syncer::SyncMergeResult local_merge_result(type());
  syncer::SyncMergeResult syncer_merge_result(type());
  if (result <= OK_FIRST_RUN) {
    state_ = RUNNING;
  } else if (result == ASSOCIATION_FAILED) {
    state_ = DISABLED;
    local_merge_result.set_error(
        syncer::SyncError(FROM_HERE,
                          syncer::SyncError::DATATYPE_ERROR,
                          "Association failed",
                          type()));
  } else if (result == UNRECOVERABLE_ERROR) {
    state_ = NOT_RUNNING;
    local_merge_result.set_error(
        syncer::SyncError(FROM_HERE,
                          syncer::SyncError::UNRECOVERABLE_ERROR,
                          "Unrecoverable error",
                          type()));
  } else if (result == NEEDS_CRYPTO) {
    state_ = NOT_RUNNING;
    local_merge_result.set_error(
        syncer::SyncError(FROM_HERE,
                          syncer::SyncError::CRYPTO_ERROR,
                          "Crypto error",
                          type()));
  } else {
    NOTREACHED();
  }
  last_start_callback_.Run(result, local_merge_result, syncer_merge_result);
}

// * -> NOT_RUNNING
void FakeDataTypeController::Stop() {
  state_ = NOT_RUNNING;
  if (!model_load_callback_.is_null()) {
    // Real data type controllers run the callback and specify "ABORTED" as an
    // error.  We should probably find a way to use the real code and mock out
    // unnecessary pieces.
    SimulateModelLoadFinishing();
  }
}

ModelType FakeDataTypeController::type() const {
  return type_;
}

std::string FakeDataTypeController::name() const {
  return ModelTypeToString(type_);
}

syncer::ModelSafeGroup FakeDataTypeController::model_safe_group() const {
  return syncer::GROUP_PASSIVE;
}

ChangeProcessor* FakeDataTypeController::GetChangeProcessor() const {
  return NULL;
}

DataTypeController::State FakeDataTypeController::state() const {
  return state_;
}

void FakeDataTypeController::OnSingleDataTypeUnrecoverableError(
    const syncer::SyncError& error) {
  if (!model_load_callback_.is_null())
    model_load_callback_.Run(type(), error);
}

bool FakeDataTypeController::ReadyForStart() const {
  return ready_for_start_;
}

void FakeDataTypeController::SetDelayModelLoad() {
  model_load_delayed_ = true;
}

void FakeDataTypeController::SetModelLoadError(syncer::SyncError error) {
  load_error_ = error;
}

void FakeDataTypeController::SimulateModelLoadFinishing() {
  model_load_callback_.Run(type(), load_error_);
}

void FakeDataTypeController::SetReadyForStart(bool ready) {
  ready_for_start_ = ready;
}

}  // namespace sync_driver
