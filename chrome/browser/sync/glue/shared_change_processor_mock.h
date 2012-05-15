// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_MOCK_H_
#define CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_MOCK_H_
#pragma once

#include "chrome/browser/sync/glue/shared_change_processor.h"
#include "sync/api/sync_change.h"
#include "sync/util/unrecoverable_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class SharedChangeProcessorMock : public SharedChangeProcessor {
 public:
  SharedChangeProcessorMock();

  MOCK_METHOD4(Connect, base::WeakPtr<SyncableService>(
      ProfileSyncComponentsFactory*,
      ProfileSyncService*,
      DataTypeErrorHandler*,
      syncable::ModelType));
  MOCK_METHOD0(Disconnect, bool());
  MOCK_METHOD2(ProcessSyncChanges,
               SyncError(const tracked_objects::Location&,
                         const SyncChangeList&));
  MOCK_METHOD1(GetSyncData,
               SyncError(SyncDataList*));
  MOCK_METHOD1(SyncModelHasUserCreatedNodes,
               bool(bool*));
  MOCK_METHOD0(CryptoReadyIfNecessary, bool());
  MOCK_METHOD1(ActivateDataType,
               void(browser_sync::ModelSafeGroup));

 protected:
  virtual ~SharedChangeProcessorMock();
  MOCK_METHOD2(OnUnrecoverableError, void(const tracked_objects::Location&,
                                          const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedChangeProcessorMock);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_MOCK_H_
