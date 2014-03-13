// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"

#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"

namespace sync_file_system {
namespace drive_backend {

void SequentialSyncTask::Run(scoped_ptr<SyncTaskToken> token) {
  RunSequential(SyncTaskToken::WrapToCallback(token.Pass()));
}

}  // namespace drive_backend
}  // namespace sync_file_system
