// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"

#include "base/callback.h"
#include "base/logging.h"

namespace sync_file_system {
namespace drive_backend {

RemoteToLocalSyncer::RemoteToLocalSyncer() {
  NOTIMPLEMENTED();
}

RemoteToLocalSyncer::~RemoteToLocalSyncer() {
  NOTIMPLEMENTED();
}

void RemoteToLocalSyncer::Run(const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

}  // namespace drive_backend
}  // namespace sync_file_system
