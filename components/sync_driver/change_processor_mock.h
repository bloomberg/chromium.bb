// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_CHANGE_PROCESSOR_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_CHANGE_PROCESSOR_MOCK_H_

#include "components/sync_driver/change_processor.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_driver {

class ChangeProcessorMock
    : public ChangeProcessor, public DataTypeErrorHandler{
 public:
  ChangeProcessorMock();
  virtual ~ChangeProcessorMock();
  MOCK_METHOD3(ApplyChangesFromSyncModel,
               void(const syncer::BaseTransaction*, int64,
                    const syncer::ImmutableChangeRecordList&));
  MOCK_METHOD0(CommitChangesFromSyncModel, void());
  MOCK_METHOD0(StartImpl, void());
  MOCK_CONST_METHOD0(IsRunning, bool());
  MOCK_METHOD2(OnUnrecoverableError, void(const tracked_objects::Location&,
                                          const std::string&));
  MOCK_METHOD2(OnSingleDatatypeUnrecoverableError,
                     void(const tracked_objects::Location&,
                          const std::string&));
  MOCK_METHOD3(CreateAndUploadError,
                   syncer::SyncError(const tracked_objects::Location&,
                             const std::string&,
                             syncer::ModelType));

};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_CHANGE_PROCESSOR_MOCK_H_
