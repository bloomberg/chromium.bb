// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_CONTENT_UPDATE_PERFORMER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_CONTENT_UPDATE_PERFORMER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "google_apis/drive/gdata_errorcode.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class ResourceEntry;
}  // namespace google_apis

namespace drive {

class JobScheduler;
class ResourceEntry;
struct ClientContext;

namespace internal {
class FileCache;
class ResourceMetadata;
}  // namespace internal

namespace file_system {

// This class encapsulates the drive Update function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class ContentUpdatePerformer {
 public:
  ContentUpdatePerformer(base::SequencedTaskRunner* blocking_task_runner,
                         JobScheduler* scheduler,
                         internal::ResourceMetadata* metadata,
                         internal::FileCache* cache);
  ~ContentUpdatePerformer();

  // Updates a file by the given |local_id| on the Drive server by
  // uploading an updated version. Used for uploading dirty files. The file
  // should already be present in the cache.
  //
  // |callback| must not be null.
  void UpdateFileByLocalId(const std::string& local_id,
                           const ClientContext& context,
                           const FileOperationCallback& callback);

  struct LocalState;

 private:
  void UpdateFileAfterGetLocalState(const ClientContext& context,
                                    const FileOperationCallback& callback,
                                    const LocalState* local_state,
                                    FileError error);

  void UpdateFileAfterUpload(
      const FileOperationCallback& callback,
      const std::string& local_id,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* metadata_;
  internal::FileCache* cache_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ContentUpdatePerformer> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ContentUpdatePerformer);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_CONTENT_UPDATE_PERFORMER_H_
