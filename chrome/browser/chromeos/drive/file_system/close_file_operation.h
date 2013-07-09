// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CLOSE_FILE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CLOSE_FILE_OPERATION_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace drive {

class ResourceEntry;

namespace internal {
class ResourceMetadata;
}  // namespace internal

namespace file_system {

class OperationObserver;

class CloseFileOperation {
 public:
  CloseFileOperation(base::SequencedTaskRunner* blocking_task_runner,
                     OperationObserver* observer,
                     internal::ResourceMetadata* metadata,
                     std::map<base::FilePath, int>* open_files);
  ~CloseFileOperation();

  // Closes the currently opened file |file_path|.
  // |callback| must not be null.
  void CloseFile(const base::FilePath& file_path,
                 const FileOperationCallback& callback);

 private:
  // Part of CloseFile(). Called after ResourceMetadata::GetResourceEntry().
  void CloseFileAfterGetResourceEntry(const base::FilePath& file_path,
                                      const FileOperationCallback& callback,
                                      const ResourceEntry* entry,
                                      FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;
  internal::ResourceMetadata* metadata_;

  // The map from paths for opened file to the number how many the file is
  // opened. The instance is owned by FileSystem and shared with
  // OpenFileOperation.
  std::map<base::FilePath, int>* open_files_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CloseFileOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CloseFileOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CLOSE_FILE_OPERATION_H_
