// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_MOCK_H__
#pragma once

#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

ACTION_P2(InvokeCallback, datatype, callback_result) {
  if (callback_result != browser_sync::DataTypeController::OK) {
    SyncError error(FROM_HERE, "Error message", datatype);
    arg0.Run(callback_result, error);
  } else {
    arg0.Run(callback_result, SyncError());
  }
}

ACTION_P3(InvokeCallbackPointer, callback, datatype, callback_result) {
  if (callback_result != browser_sync::DataTypeController::OK) {
    SyncError error(FROM_HERE, "Error message", datatype);
    callback.Run(callback_result, error);
  } else {
    callback.Run(callback_result, SyncError());
  }
}

ACTION_P3(NotifyFromDataTypeManagerWithResult, dtm, type, result) {
  content::NotificationService::current()->Notify(
      type,
      content::Source<browser_sync::DataTypeManager>(dtm),
      content::Details<const browser_sync::DataTypeManager::ConfigureResult>(
          result));
}

ACTION_P2(NotifyFromDataTypeManager, dtm, type) {
  content::NotificationService::current()->Notify(type,
      content::Source<browser_sync::DataTypeManager>(dtm),
      content::NotificationService::NoDetails());
}

namespace browser_sync {

class DataTypeManagerMock : public DataTypeManager {
 public:
  DataTypeManagerMock();
  virtual ~DataTypeManagerMock();

  MOCK_METHOD2(Configure, void(TypeSet, sync_api::ConfigureReason));
  MOCK_METHOD2(ConfigureWithoutNigori,
               void(TypeSet, sync_api::ConfigureReason));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(controllers, const DataTypeController::TypeMap&());
  MOCK_CONST_METHOD0(state, State());

 private:
  browser_sync::DataTypeManager::ConfigureResult result_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_MOCK_H__
