// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_EVENT_OBSERVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_EVENT_OBSERVER_H_

#include <string>

#include "webkit/fileapi/syncable/sync_status_code.h"

namespace fileapi {
class FileSystemURL;
}

namespace sync_file_system {

class SyncEventObserver {
 public:
  SyncEventObserver() {}
  virtual ~SyncEventObserver() {}

  enum SyncOperation {
    // A file or directory was added.
    SYNC_OPERATION_ADD,

    // A file or directory was updated.
    SYNC_OPERATION_UPDATE,

    // A file or directory was deleted.
    SYNC_OPERATION_DELETE,
  };

  enum SyncServiceState {
    // The sync service is being initialized (e.g. restoring data from the
    // database, checking connectivity and authenticating to the service etc).
    SYNC_SERVICE_INITIALIZING,

    // The sync service has detected changes in local or remote side and is
    // performing file synchronization.
    SYNC_SERVICE_SYNCING,

    // The sync service is up and runnable but currently is idle (i.e. no
    // file activities which require sync operation in remote and local side).
    SYNC_SERVICE_IDLE,

    // The sync service is (temporarily) suspended due to some recoverable
    // errors, e.g. network is offline, the user is not authenticated, remote
    // service is down or not reachable etc. More details should be given
    // by |description| parameter in OnSyncStateUpdated (which could contain
    // service-specific details).
    // TODO(kinuko): Revisit this to see if we need a separate state for
    // 'auth required'.
    SYNC_SERVICE_TEMPORARY_UNAVAILABLE,

    // The sync service is disabled and the content will never sync.
    SYNC_SERVICE_DISABLED,
  };

  // Reports there was an error in the sync file system backend.
  virtual void OnSyncStateUpdated(fileapi::SyncServiceState state,
                                  const std::string& description) = 0;

  // Reports the file |url| was updated for |operation|
  // by the sync file system backend.
  virtual void OnFileSynced(fileapi::SyncStatusCode status,
                            SyncOperation operation,
                            const FileSystemURL& url) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncEventObserver);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_EVENT_OBSERVER_H_
