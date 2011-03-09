// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_MOCK_H__
#pragma once

#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

ACTION_P3(NotifyFromDataTypeManagerWithResult, dtm, type, result) {
  NotificationService::current()->Notify(
      type,
      Source<browser_sync::DataTypeManager>(dtm),
      Details<browser_sync::DataTypeManager::ConfigureResult>(result));
}

ACTION_P2(NotifyFromDataTypeManager, dtm, type) {
  NotificationService::current()->Notify(type,
      Source<browser_sync::DataTypeManager>(dtm),
      NotificationService::NoDetails());
}

namespace browser_sync {

class DataTypeManagerMock : public DataTypeManager {
 public:
  DataTypeManagerMock();
  virtual ~DataTypeManagerMock();

  MOCK_METHOD1(Configure, void(const TypeSet&));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(controllers, const DataTypeController::TypeMap&());
  MOCK_METHOD0(state, State());

 private:
  DataTypeManager::ConfigureResult result_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_MOCK_H__
