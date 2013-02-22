// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_STATUS_OBSERVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_STATUS_OBSERVER_H_

#include "base/basictypes.h"
#include "webkit/fileapi/syncable/sync_action.h"
#include "webkit/fileapi/syncable/sync_direction.h"
#include "webkit/fileapi/syncable/sync_file_status.h"

namespace fileapi {
class FileSystemURL;
}

namespace sync_file_system {

class FileStatusObserver {
 public:
  FileStatusObserver() {}
  virtual ~FileStatusObserver() {}

  virtual void OnFileStatusChanged(const fileapi::FileSystemURL& url,
                                   SyncFileStatus sync_status,
                                   SyncAction action_taken,
                                   SyncDirection direction) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileStatusObserver);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_STATUS_OBSERVER_H_
