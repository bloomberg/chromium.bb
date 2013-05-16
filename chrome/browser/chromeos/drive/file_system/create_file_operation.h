// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_FILE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_FILE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace base {
class FilePath;
}

namespace google_apis {
class ResourceEntry;
}

namespace drive {

namespace internal {
class FileCache;
}  // namespace internal

class JobScheduler;
class ResourceEntry;

namespace file_system {

class OperationObserver;

// This class encapsulates the drive CreateFile function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class CreateFileOperation {
 public:
  CreateFileOperation(
      JobScheduler* job_scheduler,
      internal::FileCache* cache,
      internal::ResourceMetadata* metadata,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      OperationObserver* observer);
  ~CreateFileOperation();

  // Creates an empty file at |file_path| in the remote server. When the file
  // already exists at that path, the operation fails if |is_exclusive| is true,
  // and it succeeds without doing anything if the flag is false.
  //
  // |callback| must not be null.
  void CreateFile(const base::FilePath& file_path,
                  bool is_exclusive,
                  const FileOperationCallback& callback);

 private:
  void CreateFileAfterGetResourceEntry(
      const base::FilePath& file_path,
      bool is_exclusive,
      const FileOperationCallback& callback,
      scoped_ptr<EntryInfoPairResult> pair_result);

  void CreateFileAfterGetMimeType(const base::FilePath& file_path,
                                  const std::string& parent_resource_id,
                                  const FileOperationCallback& callback,
                                  const std::string* content_type,
                                  bool got_content_type);

  void CreateFileAfterUpload(
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  void CreateFileAfterAddToMetadata(const ResourceEntry& entry,
                                    const FileOperationCallback& callback,
                                    FileError error,
                                    const base::FilePath& drive_path);

  JobScheduler* job_scheduler_;
  internal::FileCache* cache_;
  internal::ResourceMetadata* metadata_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CreateFileOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CreateFileOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_FILE_OPERATION_H_
