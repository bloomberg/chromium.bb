// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_WRITE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_WRITE_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"

namespace base {
class FilePath;
}

namespace drive {

// This class provides higher level operations for writing to Drive files over
// FileSystemInterface.
class FileWriteHelper {
 public:
  explicit FileWriteHelper(FileSystemInterface* file_system);
  ~FileWriteHelper();

  // Prepares a local temporary file path and passes it to |callback| on the
  // blocking thread pool that allows file operations. The modification to
  // the file is reflected to GData |path|. If |path| does not exist, a new
  // file is created.
  //
  // Must be called from UI thread.
  void PrepareWritableFileAndRun(const base::FilePath& path,
                                 const OpenFileCallback& callback);

 private:
  // Part of PrepareWritableFilePathAndRun(). It tries CreateFile for the case
  // file does not exist yet, does OpenFile to download and mark the file as
  // dirty, runs |callback|, and finally calls CloseFile.
  void PrepareWritableFileAndRunAfterCreateFile(
      const base::FilePath& file_path,
      const OpenFileCallback& callback,
      FileError result);
  void PrepareWritableFileAndRunAfterOpenFile(
      const base::FilePath& file_path,
      const OpenFileCallback& callback,
      FileError result,
      const base::FilePath& local_cache_path);
  void PrepareWritableFileAndRunAfterCallback(const base::FilePath& file_path);

  // File system owned by DriveSystemService.
  FileSystemInterface* file_system_;

  // WeakPtrFactory bound to the UI thread.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileWriteHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileWriteHelper);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_WRITE_HELPER_H_
