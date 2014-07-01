// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_

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

namespace drive {

class ResourceEntry;

namespace internal {
class FileCache;
class ResourceMetadata;
}  // namespace internal

namespace file_system {

class OperationObserver;

// This class encapsulates the drive Remove function.  It is responsible for
// moving the removed entry to the trash.
class RemoveOperation {
 public:
  RemoveOperation(base::SequencedTaskRunner* blocking_task_runner,
                  OperationObserver* observer,
                  internal::ResourceMetadata* metadata,
                  internal::FileCache* cache);
  ~RemoveOperation();

  // Removes the resource at |path|. If |path| is a directory and |is_recursive|
  // is set, it recursively removes all the descendants. If |is_recursive| is
  // not set, it succeeds only when the directory is empty.
  //
  // |callback| must not be null.
  void Remove(const base::FilePath& path,
              bool is_recursive,
              const FileOperationCallback& callback);

 private:
  // Part of Remove(). Called after UpdateLocalState() completion.
  void RemoveAfterUpdateLocalState(const FileOperationCallback& callback,
                                   const std::string* local_id,
                                   const ResourceEntry* entry,
                                   const base::FilePath* changed_directory_path,
                                   FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;
  internal::ResourceMetadata* metadata_;
  internal::FileCache* cache_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RemoveOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(RemoveOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_
