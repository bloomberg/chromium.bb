// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host_mock.h"

namespace browser_sync {

ACTION(InvokeTask) {
  arg2->Run();
  delete arg2;
}

SyncBackendHostMock::SyncBackendHostMock() {
  // By default, the RequestPause and RequestResume methods will
  // send the confirmation notification and return true.
  ON_CALL(*this, RequestPause()).
      WillByDefault(testing::DoAll(Notify(NotificationType::SYNC_PAUSED),
                                   testing::Return(true)));
  ON_CALL(*this, RequestResume()).
      WillByDefault(testing::DoAll(Notify(NotificationType::SYNC_RESUMED),
                                   testing::Return(true)));

  // By default, invoke the ready callback.
  ON_CALL(*this, ConfigureDataTypes(testing::_, testing::_, testing::_)).
      WillByDefault(InvokeTask());
}

SyncBackendHostMock::~SyncBackendHostMock() {}

}  // namespace browser_sync
