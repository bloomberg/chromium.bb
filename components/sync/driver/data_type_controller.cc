// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/data_type_controller.h"

#include "components/sync/base/data_type_histogram.h"
#include "components/sync/syncable/user_share.h"

namespace syncer {

DataTypeController::DataTypeController(ModelType type) : type_(type) {}

DataTypeController::~DataTypeController() {}

bool DataTypeController::IsUnrecoverableResult(ConfigureResult result) {
  return (result == UNRECOVERABLE_ERROR);
}

bool DataTypeController::IsSuccessfulResult(ConfigureResult result) {
  return (result == OK || result == OK_FIRST_RUN);
}

bool DataTypeController::ReadyForStart() const {
  return true;
}

bool DataTypeController::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

}  // namespace syncer
