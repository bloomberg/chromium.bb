// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_ERROR_HANDLER_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_ERROR_HANDLER_H__
#pragma once

#include <string>
#include "base/location.h"

#include "sync/api/sync_error.h"
#include "sync/syncable/model_type.h"
#include "sync/util/unrecoverable_error_handler.h"

namespace browser_sync {

class DataTypeErrorHandler : public UnrecoverableErrorHandler {
 public:
  // Call this to disable a datatype while it is running. This is usually
  // called for a runtime failure that is specific to a datatype.
  virtual void OnSingleDatatypeUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) = 0;

  // This will create a SyncError object. This will also upload
  // a breakpad call stack to crash server. A sync error usually means
  // that sync has to be disabled either for that type or completely.
  virtual SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message,
      syncable::ModelType type) = 0;

 protected:
  virtual ~DataTypeErrorHandler() { }
};

}  // namespace browser_sync
#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_ERROR_HANDLER_H__

