// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_MOCK_H__
#pragma once

#include <set>

#include "base/task.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "content/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class SyncBackendHostMock : public SyncBackendHost {
 public:
  SyncBackendHostMock();
  virtual ~SyncBackendHostMock();

  MOCK_METHOD3(ConfigureDataTypes,
               void(const DataTypeController::TypeMap&,
                    const std::set<syncable::ModelType>&, CancelableTask*));
  MOCK_METHOD0(RequestPause, bool());
  MOCK_METHOD0(RequestResume, bool());
  MOCK_METHOD0(StartSyncingWithServer, void());
  MOCK_METHOD1(UpdateEnabledTypes, void(const syncable::ModelTypeSet&));
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_MOCK_H__
