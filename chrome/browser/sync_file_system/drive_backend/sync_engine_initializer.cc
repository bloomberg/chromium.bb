// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"

#include "base/callback.h"
#include "base/logging.h"

namespace sync_file_system {
namespace drive_backend {

SyncEngineInitializer::SyncEngineInitializer() {
  NOTIMPLEMENTED();
}

SyncEngineInitializer::~SyncEngineInitializer() {
  NOTIMPLEMENTED();
}

void SyncEngineInitializer::Run(const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

}  // namespace drive_backend
}  // namespace sync_file_system
