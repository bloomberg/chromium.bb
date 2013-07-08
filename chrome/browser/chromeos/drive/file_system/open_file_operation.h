// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPEN_FILE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPEN_FILE_OPERATION_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace drive {

class JobScheduler;
class ResourceEntry;

namespace internal {
class ResourceMetadata;
class FileCache;
}  // namespace internal

namespace file_system {

class DownloadOperation;
class OperationObserver;

class OpenFileOperation {
 public:
  OpenFileOperation(base::SequencedTaskRunner* blocking_task_runner,
                    OperationObserver* observer,
                    JobScheduler* scheduler,
                    internal::ResourceMetadata* metadata,
                    internal::FileCache* cache,
                    const base::FilePath& temporary_file_directory,
                    std::set<base::FilePath>* open_files);
  ~OpenFileOperation();

  // Opens the file at |file_path|. If not found, failed to open.
  // If the file is not actually downloaded, this method starts to download
  // it to the cache, and then runs |callback| upon the completation with the
  // path to the local cache file.
  void OpenFile(const base::FilePath& file_path,
                const OpenFileCallback& callback);

 private:
  // Part of OpenFile(). Called after file downloading is completed.
  void OpenFileAfterFileDownloaded(const OpenFileCallback& callback,
                                   FileError error,
                                   const base::FilePath& local_file_path,
                                   scoped_ptr<ResourceEntry> entry);

  // Part of OpenFile(). Called after the updating of the local state.
  void OpenFileAfterUpdateLocalState(const OpenFileCallback& callback,
                                     const base::FilePath* local_file_path,
                                     FileError error);

  // Part of OpenFile(). Called at the end of OpenFile() regardless of whether
  // it is successfully done or not.
  void FinalizeOpenFile(const base::FilePath& drive_file_path,
                        const OpenFileCallback& callback,
                        FileError result,
                        const base::FilePath& local_file_path);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  internal::FileCache* cache_;

  scoped_ptr<DownloadOperation> download_operation_;

  // The set of paths for opened files. The instance is owned by FileSystem
  // and it is shared with CloseFileOperation.
  std::set<base::FilePath>* open_files_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<OpenFileOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(OpenFileOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPEN_FILE_OPERATION_H_
