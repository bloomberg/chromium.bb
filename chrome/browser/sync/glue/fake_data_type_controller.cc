// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/sync/glue/fake_data_type_controller.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::ModelType;
namespace browser_sync {

FakeDataTypeController::FakeDataTypeController(ModelType type)
      : state_(NOT_RUNNING), model_load_delayed_(false), type_(type) {}

FakeDataTypeController::~FakeDataTypeController() {
}

// NOT_RUNNING ->MODEL_LOADED |MODEL_STARTING.
void FakeDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  if (state_ != NOT_RUNNING) {
    ADD_FAILURE();
    return;
  }

  if (model_load_delayed_ == false) {
    model_load_callback.Run(type(), syncer::SyncError());
    state_ = MODEL_LOADED;
  } else {
    model_load_callback_ = model_load_callback;
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
void FakeDataTypeController::FinishStart(StartResult result) {
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
        syncer::SyncError(FROM_HERE, "Association failed", type()));
  } else {
    state_ = NOT_RUNNING;
    local_merge_result.set_error(
        syncer::SyncError(FROM_HERE, "Fake error", type()));
  }
  last_start_callback_.Run(result,
                           local_merge_result,
                           syncer_merge_result);
  last_start_callback_.Reset();
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

  // The DTM still expects |last_start_callback_| to be called back.
  if (!last_start_callback_.is_null()) {
    syncer::SyncError error(FROM_HERE, "Fake error", type_);
    syncer::SyncMergeResult local_merge_result(type_);
    local_merge_result.set_error(error);
    last_start_callback_.Run(ABORTED,
                             local_merge_result,
                             syncer::SyncMergeResult(type_));
  }
}

ModelType FakeDataTypeController::type() const {
  return type_;
}

std::string FakeDataTypeController::name() const {
  return ModelTypeToString(type_);
}

// This isn't called by the DTM.
syncer::ModelSafeGroup FakeDataTypeController::model_safe_group() const {
  ADD_FAILURE();
  return syncer::GROUP_PASSIVE;
}

DataTypeController::State FakeDataTypeController::state() const {
  return state_;
}

void FakeDataTypeController::OnSingleDatatypeUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  ADD_FAILURE() << message;
}

void FakeDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  ADD_FAILURE() << message;
}

void FakeDataTypeController::SetDelayModelLoad() {
  model_load_delayed_ = true;
}

void FakeDataTypeController::SimulateModelLoadFinishing() {
  ModelLoadCallback model_load_callback = model_load_callback_;
  model_load_callback.Run(type(), syncer::SyncError());
}

}  // namespace browser_sync
