// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host_mock.h"

namespace browser_sync {

using ::testing::_;

ACTION(InvokeTask) {
  arg3.Run(syncable::ModelTypeSet());
}

SyncBackendHostMock::SyncBackendHostMock() {
  // By default, invoke the ready callback.
  ON_CALL(*this, ConfigureDataTypes(_, _, _, _, _)).
      WillByDefault(InvokeTask());
}

SyncBackendHostMock::~SyncBackendHostMock() {}

}  // namespace browser_sync
