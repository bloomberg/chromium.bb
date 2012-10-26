// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_MOCK_H__

#include "chrome/browser/sync/glue/change_processor.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class ChangeProcessorMock
    : public ChangeProcessor, public DataTypeErrorHandler{
 public:
  ChangeProcessorMock();
  virtual ~ChangeProcessorMock();
  MOCK_METHOD3(ApplyChangesFromSyncModel,
               void(const syncer::BaseTransaction*, int64,
                    const syncer::ImmutableChangeRecordList&));
  MOCK_METHOD0(CommitChangesFromSyncModel, void());
  MOCK_METHOD1(StartImpl, void(Profile*));
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

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_MOCK_H__
