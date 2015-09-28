// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/proxy_data_type_controller.h"

#include "sync/api/sync_merge_result.h"

namespace sync_driver {

ProxyDataTypeController::ProxyDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    syncer::ModelType type)
    : DataTypeController(ui_thread, base::Closure()),
      state_(NOT_RUNNING),
      type_(type) {
  DCHECK(syncer::ProxyTypes().Has(type_));
}

ProxyDataTypeController::~ProxyDataTypeController() {
}

void ProxyDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  state_ = MODEL_LOADED;
  model_load_callback.Run(type(), syncer::SyncError());
}

void ProxyDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  syncer::SyncMergeResult local_merge_result(type_);
  syncer::SyncMergeResult syncer_merge_result(type_);
  state_ = RUNNING;
  start_callback.Run(DataTypeController::OK,
                     local_merge_result,
                     syncer_merge_result);
}

void ProxyDataTypeController::Stop() {
  state_ = NOT_RUNNING;
}

syncer::ModelType ProxyDataTypeController::type() const {
  DCHECK(syncer::ProxyTypes().Has(type_));
  return type_;
}

std::string ProxyDataTypeController::name() const {
  // For logging only.
  return syncer::ModelTypeToString(type());
}

DataTypeController::State ProxyDataTypeController::state() const {
  return state_;
}

void ProxyDataTypeController::OnSingleDataTypeUnrecoverableError(
    const syncer::SyncError& error) {
  NOTIMPLEMENTED();
}

void ProxyDataTypeController::ActivateDataType(
    BackendDataTypeConfigurer* configurer) {}

void ProxyDataTypeController::DeactivateDataType(
    BackendDataTypeConfigurer* configurer) {}

}  // namespace sync_driver
