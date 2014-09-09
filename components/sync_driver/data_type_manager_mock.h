// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_MOCK_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_MOCK_H__

#include "components/sync_driver/data_type_manager.h"
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_driver {

class DataTypeManagerMock : public DataTypeManager {
 public:
  DataTypeManagerMock();
  virtual ~DataTypeManagerMock();

  MOCK_METHOD2(Configure, void(syncer::ModelTypeSet, syncer::ConfigureReason));
  MOCK_METHOD1(ReenableType, void(syncer::ModelType));
  MOCK_METHOD0(ResetDataTypeErrors, void());
  MOCK_METHOD2(PurgeForMigration, void(syncer::ModelTypeSet,
                                       syncer::ConfigureReason));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(controllers, const DataTypeController::TypeMap&());
  MOCK_CONST_METHOD0(state, State());

 private:
  DataTypeManager::ConfigureResult result_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_MOCK_H__
