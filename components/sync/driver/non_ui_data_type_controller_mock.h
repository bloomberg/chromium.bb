// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_MOCK_H_

#include <string>

#include "components/sync/driver/non_ui_data_type_controller.h"
#include "components/sync/model/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class NonUIDataTypeControllerMock : public NonUIDataTypeController {
 public:
  NonUIDataTypeControllerMock();
  virtual ~NonUIDataTypeControllerMock();

  // DataTypeController mocks.
  MOCK_METHOD1(StartAssociating, void(const StartCallback& start_callback));
  MOCK_METHOD1(LoadModels, void(const ModelLoadCallback& model_load_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(type, ModelType());
  MOCK_CONST_METHOD0(name, std::string());
  MOCK_CONST_METHOD0(state, State());

  // NonUIDataTypeController mocks.
  MOCK_METHOD0(StartModels, bool());
  MOCK_METHOD0(StopModels, void());
  MOCK_METHOD2(PostTaskOnModelThread,
               bool(const tracked_objects::Location&, const base::Closure&));
  MOCK_METHOD3(StartDone,
               void(DataTypeController::ConfigureResult result,
                    const SyncMergeResult& local_merge_result,
                    const SyncMergeResult& syncer_merge_result));
  MOCK_METHOD1(RecordStartFailure, void(ConfigureResult result));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_NON_UI_DATA_TYPE_CONTROLLER_MOCK_H_
