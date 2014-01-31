// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_FILE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_FILE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace drive {

namespace internal {
class ResourceMetadata;
}  // namespace internal

class ResourceEntry;

namespace file_system {

class OperationObserver;

// This class encapsulates the drive CreateFile function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class CreateFileOperation {
 public:
  CreateFileOperation(base::SequencedTaskRunner* blocking_task_runner,
                      OperationObserver* observer,
                      internal::ResourceMetadata* metadata);
  ~CreateFileOperation();

  // Creates an empty file at |file_path|. When the file
  // already exists at that path, the operation fails if |is_exclusive| is true,
  // and it succeeds without doing anything if the flag is false.
  // If |mime_type| is non-empty, it is used as the mime type of the entry. If
  // the parameter is empty, the type is guessed from |file_path|.
  //
  // |callback| must not be null.
  void CreateFile(const base::FilePath& file_path,
                  bool is_exclusive,
                  const std::string& mime_type,
                  const FileOperationCallback& callback);

 private:
  // Part of CreateFile(). Called after the updating local state is completed.
  void CreateFileAfterUpdateLocalState(const FileOperationCallback& callback,
                                       const base::FilePath& file_path,
                                       bool is_exclusive,
                                       ResourceEntry* entry,
                                       FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;
  internal::ResourceMetadata* metadata_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CreateFileOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CreateFileOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_FILE_OPERATION_H_
