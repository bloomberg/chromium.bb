// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_

#include "base/basictypes.h"

namespace drive {
class DriveServiceInterface;
}

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

class MetadataDatabase;

class SyncEngineContext {
 public:
  SyncEngineContext() {}
  ~SyncEngineContext() {}

  virtual drive::DriveServiceInterface* GetDriveService() = 0;
  virtual MetadataDatabase* GetMetadataDatabase() = 0;
  virtual RemoteChangeProcessor* GetRemoteChangeProcessor() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncEngineContext);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_
