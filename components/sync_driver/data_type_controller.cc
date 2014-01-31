// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/data_type_controller.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/util/data_type_histogram.h"

namespace browser_sync {

DataTypeController::DataTypeController(
    scoped_refptr<base::MessageLoopProxy> ui_thread,
    const base::Closure& error_callback)
    : base::RefCountedDeleteOnMessageLoop<DataTypeController>(ui_thread),
      error_callback_(error_callback) {
}

DataTypeController::~DataTypeController() {
}

bool DataTypeController::IsUnrecoverableResult(StartResult result) {
  return (result == UNRECOVERABLE_ERROR);
}

bool DataTypeController::IsSuccessfulResult(StartResult result) {
  return (result == OK || result == OK_FIRST_RUN);
}

syncer::SyncError DataTypeController::CreateAndUploadError(
    const tracked_objects::Location& location,
    const std::string& message,
    syncer::ModelType type) {
  if (!error_callback_.is_null())
    error_callback_.Run();
  return syncer::SyncError(location,
                           syncer::SyncError::DATATYPE_ERROR,
                           message,
                           type);
}

void DataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DVLOG(1) << "Datatype Controller failed for type "
           << ModelTypeToString(type()) << "  "
           << message << " at location "
           << from_here.ToString();
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);

  // TODO(sync): remove this once search engines triggers less errors, such as
  // due to crbug.com/130448.
  if ((type() != syncer::SEARCH_ENGINES) && (!error_callback_.is_null()))
    error_callback_.Run();
}

}  // namespace browser_sync
