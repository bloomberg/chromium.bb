// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_CHANGE_PROCESSOR_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

class FilePath;

namespace fileapi {
class FileChange;
class FileChangeSet;
class FileSystemURL;
}

namespace sync_file_system {

// Represents an interface to process one remote change and applies
// it to the local file system.
// This interface is to be implemented/backed by LocalSyncFileService.
class RemoteChangeProcessor {
 public:
  typedef base::Callback<void(
      fileapi::SyncStatusCode status,
      fileapi::FileChangeSet& changes)> PrepareChangeCallback;

  RemoteChangeProcessor() {}
  virtual ~RemoteChangeProcessor() {}

  // This must be called before processing the change for the |url|.
  // This tries to lock the target |url| and returns the local changes
  // if any. (The change returned by the callback is to make a decision
  // on conflict resolution, but NOT for applying local changes to the remote,
  // which is supposed to be done by LocalChangeProcessor)
  virtual void PrepareForProcessRemoteChange(
      const fileapi::FileSystemURL& url,
      const PrepareChangeCallback& callback) = 0;

  // This is called to apply the remote |change|. If the change type is
  // ADD_OR_UPDATE for a file, |local_path| needs to point to a
  // local file path that contains the latest file image (e.g. a path
  // to a temporary file which has the data downloaded from the server).
  // This may fail with an error but should NOT result in a conflict
  // (as we must have checked the change status in PrepareRemoteSync and
  // have disabled any further writing).
  virtual void ApplyRemoteChange(
      const fileapi::FileChange& change,
      const FilePath& local_path,
      const fileapi::FileSystemURL& url,
      const fileapi::StatusCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteChangeProcessor);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_CHANGE_PROCESSOR_H_
