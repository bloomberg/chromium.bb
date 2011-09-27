// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/data_type_manager.h"

namespace browser_sync {

DataTypeManager::ConfigureResult::ConfigureResult()
    : status(UNKNOWN) {}

DataTypeManager::ConfigureResult::ConfigureResult(ConfigureStatus status,
                                                  TypeSet requested_types)
    : status(status),
      requested_types(requested_types) {
  DCHECK_EQ(OK, status);
}

DataTypeManager::ConfigureResult::ConfigureResult(
    ConfigureStatus status,
    TypeSet requested_types,
    TypeSet failed_types,
    const tracked_objects::Location& location)
    : status(status),
      requested_types(requested_types),
      failed_types(failed_types),
      location(location) {
  DCHECK_NE(OK, status);
}

DataTypeManager::ConfigureResult::~ConfigureResult() {
}

// Static.
std::string DataTypeManager::ConfigureStatusToString(ConfigureStatus status) {
  switch (status) {
    case OK:
      return "Ok";
    case ASSOCIATION_FAILED:
      return "Association Failed";
    case ABORTED:
      return "Aborted";
    case UNRECOVERABLE_ERROR:
      return "Unrecoverable Error";
    default:
      NOTREACHED();
      return std::string();
  }
}

}  // namespace browser_sync
