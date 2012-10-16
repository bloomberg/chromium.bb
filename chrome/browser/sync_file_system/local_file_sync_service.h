// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_FILE_SYNC_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

class GURL;

namespace fileapi {
class FileSystemContext;
class LocalFileSyncContext;
}

namespace sync_file_system {

// Maintains local file change tracker and sync status.
// Owned by SyncFileSystemService (which is a per-profile object).
class LocalFileSyncService {
 public:
  typedef base::Callback<void(fileapi::SyncStatusCode)> StatusCallback;

  LocalFileSyncService();
  ~LocalFileSyncService();

  void Shutdown();

  void MaybeInitializeFileSystemContext(
      const GURL& app_url,
      fileapi::FileSystemContext* file_system_context,
      const StatusCallback& callback);

 private:
  scoped_refptr<fileapi::LocalFileSyncContext> sync_context_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSyncService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_FILE_SYNC_SERVICE_H_
