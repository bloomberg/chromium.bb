// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_MOCK_H__

#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class SyncBackendHostMock : public SyncBackendHost {
 public:
  SyncBackendHostMock() {
    // By default, the RequestPause and RequestResume methods will
    // send the confirmation notification and return true.
    ON_CALL(*this, RequestPause()).
        WillByDefault(testing::DoAll(Notify(NotificationType::SYNC_PAUSED),
                                     testing::Return(true)));
    ON_CALL(*this, RequestResume()).
        WillByDefault(testing::DoAll(Notify(NotificationType::SYNC_RESUMED),
                                     testing::Return(true)));
  }

  MOCK_METHOD0(RequestPause, bool());
  MOCK_METHOD0(RequestResume, bool());
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_MOCK_H__
