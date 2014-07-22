// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_ERROR_HANDLER_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_ERROR_HANDLER_H__

#include <string>
#include "base/location.h"

#include "sync/api/sync_error.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"

namespace sync_driver {

class DataTypeErrorHandler {
 public:
  // Call this to disable a datatype while it is running. This is usually
  // called for a runtime failure that is specific to a datatype.
  virtual void OnSingleDatatypeUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) = 0;

  // This will create a syncer::SyncError object. This will also upload
  // a breakpad call stack to crash server. A sync error usually means
  // that sync has to be disabled either for that type or completely.
  virtual syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message,
      syncer::ModelType type) = 0;

 protected:
  virtual ~DataTypeErrorHandler() { }
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_ERROR_HANDLER_H__
