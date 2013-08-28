// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UPDATE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UPDATE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

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

class OperationObserver;

// This class encapsulates the drive Update function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class UpdateOperation {
 public:
  UpdateOperation(base::SequencedTaskRunner* blocking_task_runner,
                  OperationObserver* observer,
                  JobScheduler* scheduler,
                  internal::ResourceMetadata* metadata,
                  internal::FileCache* cache);
  ~UpdateOperation();

  // Flags to specify whether the md5 checksum should be compared to the value
  // stored in metadata. If |RUN_CONTENT_CHECK| is set, the check is run and the
  // upload takes place only when there is a mismatch in the checksum.
  enum ContentCheckMode {
    NO_CONTENT_CHECK,
    RUN_CONTENT_CHECK,
  };

  // Updates a file by the given |local_id| on the Drive server by
  // uploading an updated version. Used for uploading dirty files. The file
  // should already be present in the cache.
  //
  // |callback| must not be null.
  void UpdateFileByLocalId(const std::string& local_id,
                           const ClientContext& context,
                           ContentCheckMode check,
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

  void UpdateFileAfterUpdateLocalState(const FileOperationCallback& callback,
                                       const base::FilePath* drive_file_path,
                                       FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* metadata_;
  internal::FileCache* cache_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UpdateOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(UpdateOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UPDATE_OPERATION_H_
