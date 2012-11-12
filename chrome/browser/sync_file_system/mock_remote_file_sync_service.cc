// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_url.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace sync_file_system {

MockRemoteFileSyncService::MockRemoteFileSyncService() {
  typedef MockRemoteFileSyncService self;
  ON_CALL(*this, ProcessRemoteChange(_, _))
      .WillByDefault(Invoke(this, &self::ProcessRemoteChangeStub));
  ON_CALL(*this, GetLocalChangeProcessor())
      .WillByDefault(Return(local_change_processor_.get()));
}

MockRemoteFileSyncService::~MockRemoteFileSyncService() {
}
void MockRemoteFileSyncService::ProcessRemoteChangeStub(
    RemoteChangeProcessor* processor,
    const fileapi::SyncFileCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC,
                 fileapi::FileSystemURL()));
}

}  // namespace sync_file_system
