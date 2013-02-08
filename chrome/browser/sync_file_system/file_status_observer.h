// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_STATUS_OBSERVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_STATUS_OBSERVER_H_

#include "base/basictypes.h"
#include "webkit/fileapi/syncable/sync_action.h"
#include "webkit/fileapi/syncable/sync_file_status.h"

namespace fileapi {
class FileSystemURL;
}

namespace sync_file_system {

// TODO(kinuko): Cleanup these enums once we finished the migration.
enum SyncDirection {
  SYNC_DIRECTION_NONE,
  SYNC_DIRECTION_LOCAL_TO_REMOTE,
  SYNC_DIRECTION_REMOTE_TO_LOCAL,
};

class FileStatusObserver {
 public:
  FileStatusObserver() {}
  virtual ~FileStatusObserver() {}

  virtual void OnFileStatusChanged(const fileapi::FileSystemURL& url,
                                   SyncDirection direction,
                                   fileapi::SyncFileStatus sync_status,
                                   fileapi::SyncAction action_taken) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileStatusObserver);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_STATUS_OBSERVER_H_
