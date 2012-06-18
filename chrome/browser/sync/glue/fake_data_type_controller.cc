// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/sync/glue/fake_data_type_controller.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncable::ModelType;
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
    model_load_callback.Run(type(), SyncError());
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
  SyncError error;
  if (result <= OK_FIRST_RUN) {
    state_ = RUNNING;
  } else if (result == ASSOCIATION_FAILED) {
    state_ = DISABLED;
    error.Reset(FROM_HERE, "Association failed", type_);
  } else {
    state_ = NOT_RUNNING;
    error.Reset(FROM_HERE, "Fake error", type_);
  }
  last_start_callback_.Run(result, error);
  last_start_callback_.Reset();
}

// * -> NOT_RUNNING
void FakeDataTypeController::Stop() {
  state_ = NOT_RUNNING;
  // The DTM still expects |last_start_callback_| to be called back.
  if (!last_start_callback_.is_null()) {
    SyncError error(FROM_HERE, "Fake error", type_);
    last_start_callback_.Run(ABORTED, error);
  }
}

ModelType FakeDataTypeController::type() const {
  return type_;
}

std::string FakeDataTypeController::name() const {
  return ModelTypeToString(type_);
}

// This isn't called by the DTM.
browser_sync::ModelSafeGroup FakeDataTypeController::model_safe_group() const {
  ADD_FAILURE();
  return browser_sync::GROUP_PASSIVE;
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
  model_load_callback.Run(type(), SyncError());
}

}  // namespace browser_sync
