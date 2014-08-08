// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_MOCK_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_MOCK_H__

#include "components/sync_driver/data_type_controller.h"
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_driver {

class StartCallbackMock {
 public:
  StartCallbackMock();
  virtual ~StartCallbackMock();

  MOCK_METHOD3(Run, void(DataTypeController::ConfigureResult result,
                         const syncer::SyncMergeResult& local_merge_result,
                         const syncer::SyncMergeResult& syncer_merge_result));
};

class ModelLoadCallbackMock {
 public:
  ModelLoadCallbackMock();
  virtual ~ModelLoadCallbackMock();

  MOCK_METHOD2(Run, void(syncer::ModelType, syncer::SyncError));
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_MOCK_H__
