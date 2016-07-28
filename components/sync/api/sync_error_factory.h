// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_API_SYNC_ERROR_FACTORY_H_
#define COMPONENTS_SYNC_API_SYNC_ERROR_FACTORY_H_

#include <string>

#include "base/location.h"
#include "components/sync/api/sync_error.h"
#include "components/sync/base/sync_export.h"

namespace syncer {

class SYNC_EXPORT SyncErrorFactory {
 public:
  SyncErrorFactory();
  virtual ~SyncErrorFactory();

  // Creates a SyncError object and uploads this call stack to breakpad.
  virtual SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_API_SYNC_ERROR_FACTORY_H_
