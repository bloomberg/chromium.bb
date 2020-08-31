// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/fake_model_type_controller_delegate.h"

#include <utility>

#include "base/callback.h"
#include "base/run_loop.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"

namespace syncer {

FakeModelTypeControllerDelegate::FakeModelTypeControllerDelegate(ModelType type)
    : type_(type) {}

FakeModelTypeControllerDelegate::~FakeModelTypeControllerDelegate() {}

void FakeModelTypeControllerDelegate::EnableManualModelStart() {
  manual_model_start_enabled_ = true;
}

void FakeModelTypeControllerDelegate::SimulateModelStartFinished() {
  DCHECK(manual_model_start_enabled_);
  if (start_callback_) {
    std::move(start_callback_)
        .Run(std::make_unique<DataTypeActivationResponse>());
  }
}

void FakeModelTypeControllerDelegate::SimulateModelError(
    const ModelError& error) {
  model_error_ = error;
  start_callback_.Reset();

  if (!error_handler_) {
    return;
  }

  error_handler_.Run(error);
  // ModelTypeController's implementation uses task-posting for errors. To
  // process the error, the posted task needs processing in a RunLoop.
  base::RunLoop().RunUntilIdle();
}

int FakeModelTypeControllerDelegate::clear_metadata_call_count() const {
  return clear_metadata_call_count_;
}

void FakeModelTypeControllerDelegate::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback callback) {
  error_handler_ = request.error_handler;

  // If the model has already experienced the error, report it immediately.
  if (model_error_) {
    error_handler_.Run(*model_error_);
    // ModelTypeController's implementation uses task-posting for errors. To
    // process the error, the posted task needs processing in a RunLoop.
    base::RunLoop().RunUntilIdle();
    return;
  }

  if (manual_model_start_enabled_) {
    // Completion will be triggered from SimulateModelStartFinished().
    start_callback_ = std::move(callback);
  } else {
    // Trigger completion immediately.
    std::move(callback).Run(std::make_unique<DataTypeActivationResponse>());
  }
}

void FakeModelTypeControllerDelegate::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  if (metadata_fate == CLEAR_METADATA) {
    ++clear_metadata_call_count_;
  }
}

void FakeModelTypeControllerDelegate::GetAllNodesForDebugging(
    ModelTypeControllerDelegate::AllNodesCallback callback) {
  std::move(callback).Run(type_, std::make_unique<base::ListValue>());
}

void FakeModelTypeControllerDelegate::GetStatusCountersForDebugging(
    StatusCountersCallback callback) {
  std::move(callback).Run(type_, StatusCounters());
}

void FakeModelTypeControllerDelegate::RecordMemoryUsageAndCountsHistograms() {}

base::WeakPtr<ModelTypeControllerDelegate>
FakeModelTypeControllerDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace syncer
