// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_TRUNCATE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_TRUNCATE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace drive {

class JobScheduler;
class ResourceEntry;

namespace internal {
class FileCache;
class ResourceMetadata;
}  // namespace internal

namespace file_system {

class OperationObserver;
class DownloadOperation;

// This class encapsulates the drive Truncate function. It is responsible for
// fetching the content from the Drive server if necessary, truncating the
// file content actually, and then notifying the file is locally modified and
// that it is necessary to upload the file to the server.
class TruncateOperation {
 public:
  TruncateOperation(base::SequencedTaskRunner* blocking_task_runner,
                    OperationObserver* observer,
                    JobScheduler* scheduler,
                    internal::ResourceMetadata* metadata,
                    internal::FileCache* cache,
                    const base::FilePath& temporary_file_directory);
  ~TruncateOperation();

  // Performs the truncate operation on the file at drive path |file_path| to
  // |length| bytes. Invokes |callback| when finished with the result of the
  // operation. |callback| must not be null.
  void Truncate(const base::FilePath& file_path,
                int64 length,
                const FileOperationCallback& callback);
 private:
  // Part of Truncate(). Called after EnsureFileDownloadedByPath() is complete.
  void TruncateAfterEnsureFileDownloadedByPath(
      int64 length,
      const FileOperationCallback& callback,
      FileError error,
      const base::FilePath& local_file_path,
      scoped_ptr<ResourceEntry> resource_entry);

  // Part of Truncate(). Called after TruncateOnBlockingPool() is complete.
  void TruncateAfterTruncateOnBlockingPool(
      const std::string& local_id,
      const FileOperationCallback& callback,
      FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;
  internal::ResourceMetadata* metadata_;
  internal::FileCache* cache_;

  scoped_ptr<DownloadOperation> download_operation_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<TruncateOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(TruncateOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_TRUNCATE_OPERATION_H_
