// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_FILE_WRITE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_FILE_WRITE_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_interface.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class FilePath;

namespace gdata {

// This class provides higher level operations for writing to Drive files over
// DriveFileSystemInterface.
class FileWriteHelper {
 public:
  explicit FileWriteHelper(DriveFileSystemInterface* file_system);
  ~FileWriteHelper();

  // Prepares a local temporary file path and passes it to |callback| on the
  // blocking thread pool that allows file operations. The modification to
  // the file is reflected to GData |path|. If |path| does not exist, a new
  // file is created.
  //
  // Must be called from UI thread.
  void PrepareWritableFileAndRun(const FilePath& path,
                                 const OpenFileCallback& callback);

 private:
  // Part of PrepareWritableFilePathAndRun(). It tries CreateFile for the case
  // file does not exist yet, does OpenFile to download and mark the file as
  // dirty, runs |callback|, and finally calls CloseFile.
  void PrepareWritableFileAndRunAfterCreateFile(
      const FilePath& file_path,
      const OpenFileCallback& callback,
      DriveFileError result);
  void PrepareWritableFileAndRunAfterOpenFile(
      const FilePath& file_path,
      const OpenFileCallback& callback,
      DriveFileError result,
      const FilePath& local_cache_path);
  void PrepareWritableFileAndRunAfterCallback(const FilePath& file_path);

  // File system owned by DriveSystemService.
  DriveFileSystemInterface* file_system_;

  // WeakPtrFactory bound to the UI thread.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileWriteHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileWriteHelper);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_FILE_WRITE_HELPER_H_
