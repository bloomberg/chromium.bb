// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_ERROR_HANDLER_MOCK_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_ERROR_HANDLER_MOCK_H__

#include "components/sync_driver/data_type_controller.h"
#include "sync/internal_api/public/base/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_driver {

class DataTypeErrorHandlerMock : public DataTypeErrorHandler {
 public:
  DataTypeErrorHandlerMock();
  virtual ~DataTypeErrorHandlerMock();
  MOCK_METHOD1(OnSingleDataTypeUnrecoverableError,
               void(const syncer::SyncError&));
  MOCK_METHOD3(CreateAndUploadError,
                   syncer::SyncError(const tracked_objects::Location&,
                             const std::string&,
                             syncer::ModelType));

};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_ERROR_HANDLER_MOCK_H__
