// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_FILE_SYNC_SERVICE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

class GURL;

namespace fileapi {
class FileSystemContext;
class LocalFileSyncContext;
}

namespace sync_file_system {

class LocalChangeProcessor;

// Maintains local file change tracker and sync status.
// Owned by SyncFileSystemService (which is a per-profile object).
class LocalFileSyncService
    : public RemoteChangeProcessor,
      public base::SupportsWeakPtr<LocalFileSyncService> {
 public:
  LocalFileSyncService();
  virtual ~LocalFileSyncService();

  void Shutdown();

  void MaybeInitializeFileSystemContext(
      const GURL& app_origin,
      const std::string& service_name,
      fileapi::FileSystemContext* file_system_context,
      const fileapi::StatusCallback& callback);

  // Synchronize one (or a set of) local change(s) to the remote server
  // using |processor|.
  // |processor| must have same or longer lifetime than this service.
  void ProcessLocalChange(LocalChangeProcessor* processor,
                          const fileapi::SyncCompletionCallback& callback);

  // RemoteChangeProcessor overrides.
  virtual void PrepareForProcessRemoteChange(
      const fileapi::FileSystemURL& url,
      const PrepareChangeCallback& callback) OVERRIDE;
  virtual void ApplyRemoteChange(
      const fileapi::FileChange& change,
      const FilePath& local_path,
      const fileapi::FileSystemURL& url,
      const fileapi::StatusCallback& callback) OVERRIDE;

 private:
  void DidInitializeFileSystemContext(
      const GURL& app_origin,
      fileapi::FileSystemContext* file_system_context,
      const fileapi::StatusCallback& callback,
      fileapi::SyncStatusCode status);

  scoped_refptr<fileapi::LocalFileSyncContext> sync_context_;

  // Origin to context map. (Assuming that as far as we're in the same
  // profile single origin wouldn't belong to multiple FileSystemContexts.)
  std::map<GURL, fileapi::FileSystemContext*> origin_to_contexts_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSyncService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_FILE_SYNC_SERVICE_H_
