// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_MOCK_H__
#pragma once

#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class NonFrontendDataTypeControllerMock : public NonFrontendDataTypeController {
 public:
  NonFrontendDataTypeControllerMock();

  // DataTypeController mocks.
  MOCK_METHOD1(Start, void(const StartCallback& start_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(enabled, bool());
  MOCK_CONST_METHOD0(type, syncable::ModelType());
  MOCK_CONST_METHOD0(name, std::string());
  MOCK_CONST_METHOD0(model_safe_group, browser_sync::ModelSafeGroup());
  MOCK_CONST_METHOD0(state, State());
  MOCK_METHOD2(OnUnrecoverableError, void(const tracked_objects::Location&,
                                          const std::string&));

  // NonFrontendDataTypeController mocks.
  MOCK_METHOD0(StartModels, bool());
  MOCK_METHOD2(PostTaskOnBackendThread,
               bool(const tracked_objects::Location&,
                    const base::Closure&));
  MOCK_METHOD0(StartAssociation, void());
  MOCK_METHOD0(CreateSyncComponents, void());
  MOCK_METHOD2(StartFailed, void(StartResult result,
                                 const SyncError& error));
  MOCK_METHOD3(StartDone, void(DataTypeController::StartResult result,
                               DataTypeController::State new_state,
                               const SyncError& error));
  MOCK_METHOD3(StartDoneImpl, void(DataTypeController::StartResult result,
                                   DataTypeController::State new_state,
                                   const SyncError& error));
  MOCK_METHOD0(StopModels, void());
  MOCK_METHOD0(StopAssociation, void());
  MOCK_METHOD2(OnUnrecoverableErrorImpl, void(const tracked_objects::Location&,
                                              const std::string&));
  MOCK_METHOD2(RecordUnrecoverableError, void(const tracked_objects::Location&,
                                              const std::string&));
  MOCK_METHOD1(RecordAssociationTime, void(base::TimeDelta time));
  MOCK_METHOD1(RecordStartFailure, void(StartResult result));

 protected:
  virtual ~NonFrontendDataTypeControllerMock();
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_MOCK_H__
