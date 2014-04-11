// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_MOCK_H_
#define CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_MOCK_H_

#include "chrome/browser/sync/glue/shared_change_processor.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class SharedChangeProcessorMock : public SharedChangeProcessor {
 public:
  SharedChangeProcessorMock();

  MOCK_METHOD5(Connect, base::WeakPtr<syncer::SyncableService>(
      ProfileSyncComponentsFactory*,
      ProfileSyncService*,
      DataTypeErrorHandler*,
      syncer::ModelType,
      const base::WeakPtr<syncer::SyncMergeResult>&));
  MOCK_METHOD0(Disconnect, bool());
  MOCK_METHOD2(ProcessSyncChanges,
               syncer::SyncError(const tracked_objects::Location&,
                         const syncer::SyncChangeList&));
  MOCK_CONST_METHOD2(GetAllSyncDataReturnError,
                     syncer::SyncError(syncer::ModelType,
                                       syncer::SyncDataList*));
  MOCK_METHOD0(GetSyncCount, int());
  MOCK_METHOD1(SyncModelHasUserCreatedNodes,
               bool(bool*));
  MOCK_METHOD0(CryptoReadyIfNecessary, bool());
  MOCK_CONST_METHOD1(GetDataTypeContext, bool(std::string*));
  MOCK_METHOD1(ActivateDataType,
               void(syncer::ModelSafeGroup));

 protected:
  virtual ~SharedChangeProcessorMock();
  MOCK_METHOD2(OnUnrecoverableError, void(const tracked_objects::Location&,
                                          const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedChangeProcessorMock);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_MOCK_H_
