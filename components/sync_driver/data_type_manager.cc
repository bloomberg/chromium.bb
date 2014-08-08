// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/data_type_manager.h"

namespace sync_driver {

DataTypeManager::ConfigureResult::ConfigureResult()
    : status(UNKNOWN) {
}

DataTypeManager::ConfigureResult::ConfigureResult(
    ConfigureStatus status,
    syncer::ModelTypeSet requested_types)
    : status(status), requested_types(requested_types) {
}

DataTypeManager::ConfigureResult::~ConfigureResult() {
}

// Static.
std::string DataTypeManager::ConfigureStatusToString(ConfigureStatus status) {
  switch (status) {
    case OK:
      return "Ok";
    case ABORTED:
      return "Aborted";
    case UNRECOVERABLE_ERROR:
      return "Unrecoverable Error";
    case UNKNOWN:
      NOTREACHED();
      return std::string();
  }
  return std::string();
}

}  // namespace sync_driver
