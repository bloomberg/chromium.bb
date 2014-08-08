// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_FRONTEND_DATA_TYPE_CONTROLLER_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_FRONTEND_DATA_TYPE_CONTROLLER_MOCK_H__

#include "chrome/browser/sync/glue/frontend_data_type_controller.h"
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class FrontendDataTypeControllerMock : public FrontendDataTypeController {
 public:
  FrontendDataTypeControllerMock();

  // DataTypeController mocks.
  MOCK_METHOD1(StartAssociating,
               void(const StartCallback& start_callback));
  MOCK_METHOD1(LoadModels, void(const ModelLoadCallback& model_load_callback));
  MOCK_METHOD0(OnModelLoaded, void());

  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(enabled, bool());
  MOCK_CONST_METHOD0(type, syncer::ModelType());
  MOCK_CONST_METHOD0(name, std::string());
  MOCK_CONST_METHOD0(model_safe_group, syncer::ModelSafeGroup());
  MOCK_CONST_METHOD0(state, State());
  MOCK_METHOD2(OnUnrecoverableError, void(const tracked_objects::Location&,
                                          const std::string&));

  // FrontendDataTypeController mocks.
  MOCK_METHOD0(StartModels, bool());
  MOCK_METHOD0(Associate, bool());
  MOCK_METHOD0(CreateSyncComponents, void());
  MOCK_METHOD2(StartFailed, void(ConfigureResult result,
                                 const syncer::SyncError& error));
  MOCK_METHOD1(FinishStart, void(ConfigureResult result));
  MOCK_METHOD0(CleanUpState, void());
  MOCK_CONST_METHOD0(model_associator, sync_driver::AssociatorInterface*());
  MOCK_METHOD1(set_model_associator,
               void(sync_driver::AssociatorInterface* associator));
  MOCK_CONST_METHOD0(change_processor, sync_driver::ChangeProcessor*());
  MOCK_METHOD1(set_change_processor,
               void(sync_driver::ChangeProcessor* processor));
  MOCK_METHOD2(RecordUnrecoverableError, void(const tracked_objects::Location&,
                                              const std::string&));
  MOCK_METHOD1(RecordAssociationTime, void(base::TimeDelta time));
  MOCK_METHOD1(RecordStartFailure, void(ConfigureResult result));

 protected:
  virtual ~FrontendDataTypeControllerMock();
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FRONTEND_DATA_TYPE_CONTROLLER_MOCK_H__
