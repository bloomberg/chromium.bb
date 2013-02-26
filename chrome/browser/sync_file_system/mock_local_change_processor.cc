// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_local_change_processor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/file_change.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace sync_file_system {

MockLocalChangeProcessor::MockLocalChangeProcessor() {
  ON_CALL(*this, ApplyLocalChange(_, _, _, _))
      .WillByDefault(Invoke(this,
                            &MockLocalChangeProcessor::ApplyLocalChangeStub));
}

MockLocalChangeProcessor::~MockLocalChangeProcessor() {
}

void MockLocalChangeProcessor::ApplyLocalChangeStub(
    const FileChange& change,
    const base::FilePath& local_file_path,
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
}

}  // namespace sync_file_system
