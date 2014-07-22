// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/proxy_data_type_controller.h"

namespace sync_driver {

ProxyDataTypeController::ProxyDataTypeController(
    scoped_refptr<base::MessageLoopProxy> ui_thread,
    syncer::ModelType type)
    : DataTypeController(ui_thread, base::Closure(), DisableTypeCallback()),
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

syncer::ModelSafeGroup ProxyDataTypeController::model_safe_group() const {
  DCHECK(syncer::ProxyTypes().Has(type_));
  return syncer::GROUP_PASSIVE;
}

ChangeProcessor* ProxyDataTypeController::GetChangeProcessor() const {
  return NULL;
}

std::string ProxyDataTypeController::name() const {
  // For logging only.
  return syncer::ModelTypeToString(type());
}

DataTypeController::State ProxyDataTypeController::state() const {
  return state_;
}

void ProxyDataTypeController::OnSingleDatatypeUnrecoverableError(
    const tracked_objects::Location& from_here, const std::string& message) {
  NOTIMPLEMENTED();
}

void ProxyDataTypeController::OnModelLoaded() {
  NOTIMPLEMENTED();
}

}  // namespace sync_driver
