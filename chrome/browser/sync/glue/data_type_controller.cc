// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"

#include "chrome/browser/sync/glue/data_type_controller.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/util/data_type_histogram.h"

namespace browser_sync {

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
  ChromeReportUnrecoverableError();
  return syncer::SyncError(location, message, type);
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
  if (type() != syncer::SEARCH_ENGINES)
    ChromeReportUnrecoverableError();
}

}  // namespace browser_sync
