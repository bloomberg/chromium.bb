// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_EVENT_OBSERVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_EVENT_OBSERVER_H_

#include <string>

#include "chrome/browser/sync_file_system/file_status_observer.h"

class GURL;

namespace fileapi {
class FileSystemURL;
}

namespace sync_file_system {

class SyncEventObserver {
 public:
  SyncEventObserver() {}
  virtual ~SyncEventObserver() {}

  enum SyncServiceState {
    // The sync service is up and running.
    SYNC_SERVICE_RUNNING,

    // The sync service is not synchronizing files because the remote service
    // needs to be authenticated by the user to proceed.
    SYNC_SERVICE_AUTHENTICATION_REQUIRED,

    // The sync service is not synchronizing files because the remote service
    // is (temporarily) unavailable due to some recoverable errors, e.g.
    // network is offline, the remote service is down or not
    // reachable etc. More details should be given by |description| parameter
    // in OnSyncStateUpdated (which could contain service-specific details).
    SYNC_SERVICE_TEMPORARY_UNAVAILABLE,

    // The sync service is disabled and the content will never sync.
    SYNC_SERVICE_DISABLED,
  };

  // Reports there was a state change in the sync file system backend.
  // If |app_origin| is empty, then broadcast to all registered apps.
  virtual void OnSyncStateUpdated(const GURL& app_origin,
                                  SyncServiceState state,
                                  const std::string& description) = 0;

  // Reports the file |url| was updated and resulted in |result|
  // by the sync file system backend.
  virtual void OnFileSynced(const fileapi::FileSystemURL& url,
                            SyncFileStatus status,
                            SyncAction action,
                            SyncDirection direction) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncEventObserver);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_EVENT_OBSERVER_H_
