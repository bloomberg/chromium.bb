// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPEN_FILE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPEN_FILE_OPERATION_H_

#include <map>

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

class JobScheduler;
class ResourceEntry;

namespace internal {
class ResourceMetadata;
class FileCache;
}  // namespace internal

namespace file_system {

class CreateFileOperation;
class DownloadOperation;
class OperationDelegate;

class OpenFileOperation {
 public:
  OpenFileOperation(base::SequencedTaskRunner* blocking_task_runner,
                    OperationDelegate* delegate,
                    JobScheduler* scheduler,
                    internal::ResourceMetadata* metadata,
                    internal::FileCache* cache,
                    const base::FilePath& temporary_file_directory);
  ~OpenFileOperation();

  // Opens the file at |file_path|.
  // If the file is not actually downloaded, this method starts
  // to download it to the cache, and then runs |callback| upon the
  // completion with the path to the local cache file.
  // See also the definition of OpenMode for its meaning.
  // If |mime_type| is non empty and the file is created by this OpenFile()
  // call, the mime type is used as the file's property.
  // |callback| must not be null.
  void OpenFile(const base::FilePath& file_path,
                OpenMode open_mode,
                const std::string& mime_type,
                const OpenFileCallback& callback);

 private:
  // Part of OpenFile(). Called after file creation is completed.
  void OpenFileAfterCreateFile(const base::FilePath& file_path,
                               const OpenFileCallback& callback,
                               FileError error);

  // Part of OpenFile(). Called after file downloading is completed.
  void OpenFileAfterFileDownloaded(const OpenFileCallback& callback,
                                   FileError error,
                                   const base::FilePath& local_file_path,
                                   scoped_ptr<ResourceEntry> entry);

  // Part of OpenFile(). Called after opening the cache file.
  void OpenFileAfterOpenForWrite(
      const base::FilePath& local_file_path,
      const std::string& local_id,
      const OpenFileCallback& callback,
      scoped_ptr<base::ScopedClosureRunner>* file_closer,
      FileError error);

  // Closes the file with |local_id|.
  void CloseFile(const std::string& local_id,
                 scoped_ptr<base::ScopedClosureRunner> file_closer);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationDelegate* delegate_;
  internal::FileCache* cache_;

  scoped_ptr<CreateFileOperation> create_file_operation_;
  scoped_ptr<DownloadOperation> download_operation_;

  // The map from local id for an opened file to the number how many times
  // the file is opened.
  std::map<std::string, int> open_files_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<OpenFileOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(OpenFileOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPEN_FILE_OPERATION_H_
