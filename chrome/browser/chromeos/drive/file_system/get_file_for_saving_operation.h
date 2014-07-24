// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_GET_FILE_FOR_SAVING_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_GET_FILE_FOR_SAVING_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"

namespace base {
class FilePath;
class ScopedClosureRunner;
class SequencedTaskRunner;
}  // namespace base

namespace drive {
namespace internal {
class FileCache;
class FileWriteWatcher;
class ResourceMetadata;
}  // namespace internal

class EventLogger;
class JobScheduler;
class ResourceEntry;

namespace file_system {

class CreateFileOperation;
class DownloadOperation;
class OperationDelegate;

// Implements GetFileForSaving() operation that prepares a local cache for
// a Drive file whose next modification is monitored and notified to the
// OperationDelegate.
// TODO(kinaba): crbug.com/269424: we might want to monitor all the changes
// to the cache directory, not just the one immediately after the save dialog.
class GetFileForSavingOperation {
 public:
  GetFileForSavingOperation(EventLogger* logger,
                            base::SequencedTaskRunner* blocking_task_runner,
                            OperationDelegate* delegate,
                            JobScheduler* scheduler,
                            internal::ResourceMetadata* metadata,
                            internal::FileCache* cache,
                            const base::FilePath& temporary_file_directory);
  ~GetFileForSavingOperation();

  // Makes sure that |file_path| in the file system is available in the local
  // cache, and marks it as dirty. The next modification to the cache file is
  // watched and is automatically notified to the delegate. If the entry is not
  // present in the file system, it is created.
  void GetFileForSaving(const base::FilePath& file_path,
                        const GetFileCallback& callback);

  internal::FileWriteWatcher* file_write_watcher_for_testing() {
    return file_write_watcher_.get();
  }

 private:
  void GetFileForSavingAfterCreate(const base::FilePath& file_path,
                                   const GetFileCallback& callback,
                                   FileError error);
  void GetFileForSavingAfterDownload(const GetFileCallback& callback,
                                     FileError error,
                                     const base::FilePath& cache_path,
                                     scoped_ptr<ResourceEntry> entry);
  void GetFileForSavingAfterOpenForWrite(
      const GetFileCallback& callback,
      const base::FilePath& cache_path,
      scoped_ptr<ResourceEntry> entry,
      scoped_ptr<base::ScopedClosureRunner>* file_closer,
      FileError error);
  void GetFileForSavingAfterWatch(const GetFileCallback& callback,
                                  const base::FilePath& cache_path,
                                  scoped_ptr<ResourceEntry> entry,
                                  bool success);
  // Called when the cache file for |local_id| is written.
  void OnWriteEvent(const std::string& local_id,
                    scoped_ptr<base::ScopedClosureRunner> file_closer);

  EventLogger* logger_;
  scoped_ptr<CreateFileOperation> create_file_operation_;
  scoped_ptr<DownloadOperation> download_operation_;
  scoped_ptr<internal::FileWriteWatcher> file_write_watcher_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationDelegate* delegate_;
  internal::ResourceMetadata* metadata_;
  internal::FileCache* cache_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GetFileForSavingOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(GetFileForSavingOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_GET_FILE_FOR_SAVING_OPERATION_H_
